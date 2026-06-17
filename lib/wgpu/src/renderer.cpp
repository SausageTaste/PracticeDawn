#include "renderer.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

#include <glm/gtc/type_ptr.hpp>


namespace {

    wgpu::TextureView make_depth_view(
        const wgpu::Device& device, uint32_t w, uint32_t h
    ) {
        const wgpu::TextureDescriptor desc{
            .usage = wgpu::TextureUsage::RenderAttachment,
            .size = { w, h, 1 },
            .format = wgpu::TextureFormat::Depth24Plus,
        };
        return device.CreateTexture(&desc).CreateView();
    }

}  // namespace


// SurfacePackage
namespace practice {

    void SurfacePackage::init(
        const wgpu::SurfaceDescriptor& descriptor,
        const uint32_t width,
        const uint32_t height,
        const DevicePackage& dpkg
    ) {
        surface_ = dpkg.instance_.CreateSurface(&descriptor);

        caps_ = dpkg.get_surface_caps(surface_);
        const auto alphaMode = [&]() {
            for (size_t i = 0; i < caps_.alphaModeCount; i++)
                if (caps_.alphaModes[i] ==
                    wgpu::CompositeAlphaMode::Premultiplied)
                    return wgpu::CompositeAlphaMode::Premultiplied;
            return wgpu::CompositeAlphaMode::Opaque;
        }();

        config_ = wgpu::SurfaceConfiguration{
            .device = dpkg.device_,
            .format = caps_.formats[0],
            .usage = wgpu::TextureUsage::RenderAttachment,
            .width = width,
            .height = height,
            .alphaMode = alphaMode,
        };
        surface_.Configure(&config_);
    }

    void SurfacePackage::present() const {
#if !defined(__EMSCRIPTEN__)
        surface_.Present();
#endif
    }

    void SurfacePackage::on_fbuf_resize(
        uint32_t new_width, uint32_t new_height
    ) {
        config_.width = new_width;
        config_.height = new_height;
        surface_.Configure(&config_);
    }

    wgpu::TextureView SurfacePackage::acquire_color_view() {
        tex_ = {};
        surface_.GetCurrentTexture(&tex_);

        switch (tex_.status) {
            case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
            case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
                if (!tex_.texture)
                    throw std::runtime_error(
                        "Surface returned success without a texture"
                    );
                return tex_.texture.CreateView();
            case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
                throw std::runtime_error("Timed out acquiring surface texture");
            case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
                throw std::runtime_error("Surface texture is outdated");
            case wgpu::SurfaceGetCurrentTextureStatus::Lost:
                throw std::runtime_error("Surface texture was lost");
            case wgpu::SurfaceGetCurrentTextureStatus::Error:
            default:
                throw std::runtime_error("Failed to acquire surface texture");
        }
    }

}  // namespace practice


// Mesh
namespace practice {

    void Mesh::init(
        const std::span<const Vertex> vertices,
        const std::span<const uint16_t> indices,
        const wgpu::Queue& queue,
        const wgpu::Device& device
    ) {
        // Geometry buffers
        const uint64_t kVbSize = vertices.size() * sizeof(Vertex);
        const wgpu::BufferDescriptor vbDesc{
            .usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst,
            .size = kVbSize,
        };
        vtx_buf_ = device.CreateBuffer(&vbDesc);
        queue.WriteBuffer(vtx_buf_, 0, vertices.data(), kVbSize);

        const uint64_t kIbSize = indices.size() * sizeof(uint16_t);
        if (indices.size() > std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("Mesh has too many indices");
        index_count_ = static_cast<uint32_t>(indices.size());

        const wgpu::BufferDescriptor ibDesc{
            .usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst,
            .size = kIbSize,
        };
        idx_buf_ = device.CreateBuffer(&ibDesc);
        queue.WriteBuffer(idx_buf_, 0, indices.data(), kIbSize);
    }

}  // namespace practice


// Actor
namespace practice {

    void Actor::init(
        const wgpu::BindGroupLayout& group_layout, const wgpu::Device& device
    ) {
        // MVP uniform buffer
        const wgpu::BufferDescriptor ubDesc{
            .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
            .size = sizeof(glm::mat4),
        };
        ubuf_ = device.CreateBuffer(&ubDesc);

        // Bind group
        const wgpu::BindGroupEntry bgEntry{
            .binding = 0,
            .buffer = ubuf_,
            .size = sizeof(glm::mat4),
        };
        const wgpu::BindGroupDescriptor bgDesc{
            .layout = group_layout,
            .entryCount = 1,
            .entries = &bgEntry,
        };
        bind_group_ = device.CreateBindGroup(&bgDesc);
    }

    void Actor::update_ubuf(const glm::mat4& mvp, const wgpu::Queue& queue) {
        queue.WriteBuffer(ubuf_, 0, glm::value_ptr(mvp), sizeof(glm::mat4));
    }

}  // namespace practice


// Renderer
namespace practice {

    void Renderer::init() { device_pkg_.init(); }

    void Renderer::init_async(std::function<void()> on_ready) {
        device_pkg_.init_async(std::move(on_ready));
    }

    void Renderer::create_surface(
        const wgpu::SurfaceDescriptor& descriptor,
        uint32_t width,
        uint32_t height
    ) {
        surface_pkg_.init(descriptor, width, height, device_pkg_);
    }

    void Renderer::create_render_pass(const std::filesystem::path& asset_dir) {
        auto& device = device_pkg_.device_;
        queue_ = device.GetQueue();

        // Shader
        const auto shader_path = asset_dir / "shaders" / "cube.wgsl";
        std::ifstream shader_file(shader_path);
        if (!shader_file.is_open())
            throw std::runtime_error(
                "Failed to open shader: " + shader_path.string()
            );
        const std::string shader_code(
            (std::istreambuf_iterator<char>(shader_file)),
            std::istreambuf_iterator<char>()
        );
        wgpu::ShaderSourceWGSL wgslSource{};
        wgslSource.code = shader_code.c_str();
        const wgpu::ShaderModuleDescriptor shaderDesc{
            .nextInChain = &wgslSource
        };
        const auto shader = device_pkg_.device_.CreateShaderModule(&shaderDesc);

        // Bind group layout (uniform at binding 0, visible to vertex stage)
        const wgpu::BindGroupLayoutEntry bglEntry{
            .binding = 0,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer = { .type = wgpu::BufferBindingType::Uniform },
        };
        const wgpu::BindGroupLayoutDescriptor bglDesc{
            .entryCount = 1,
            .entries = &bglEntry,
        };
        const auto bindGroupLayout = device.CreateBindGroupLayout(&bglDesc);

        const wgpu::PipelineLayoutDescriptor layoutDesc{
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &bindGroupLayout,
        };
        const auto pipelineLayout = device.CreatePipelineLayout(&layoutDesc);

        // Render pipeline
        constexpr wgpu::TextureFormat kDepthFormat =
            wgpu::TextureFormat::Depth24Plus;
        depth_view_ = ::make_depth_view(
            device, surface_pkg_.width(), surface_pkg_.height()
        );

        const std::array<wgpu::VertexAttribute, 2> vertexAttribs{ {
            { .format = wgpu::VertexFormat::Float32x3,
              .offset = 0,
              .shaderLocation = 0 },
            { .format = wgpu::VertexFormat::Float32x4,
              .offset = 12,
              .shaderLocation = 1 },
        } };
        const wgpu::VertexBufferLayout vertexLayout{
            .arrayStride = sizeof(Mesh::Vertex),
            .attributeCount = vertexAttribs.size(),
            .attributes = vertexAttribs.data(),
        };
        const wgpu::ColorTargetState colorTarget{
            .format = surface_pkg_.caps().formats[0]
        };
        const wgpu::FragmentState fragmentState{
            .module = shader,
            .entryPoint = "fs_main",
            .targetCount = 1,
            .targets = &colorTarget,
        };
        const wgpu::DepthStencilState depthStencil{
            .format = kDepthFormat,
            .depthWriteEnabled = wgpu::OptionalBool::True,
            .depthCompare = wgpu::CompareFunction::Less,
        };
        const wgpu::RenderPipelineDescriptor pipelineDesc{
                .layout = pipelineLayout,
                .vertex = {
                    .module = shader,
                    .entryPoint = "vs_main",
                    .bufferCount = 1,
                    .buffers = &vertexLayout,
                },
                .depthStencil = &depthStencil,
                .fragment = &fragmentState,
            };
        pipeline_ = device.CreateRenderPipeline(&pipelineDesc);
    }

    void Renderer::tick() {
        const double aspect = static_cast<double>(surface_pkg_.width()) /
                              static_cast<double>(surface_pkg_.height());
        const auto proj = glm::perspectiveRH_ZO(
            glm::radians(60.0), aspect, 0.1, 1000.0
        );
        const auto view = scene_.camera_view_.make_view_mat();
        const auto pv = proj * view;

        for (auto& meshActor : scene_.mesh_actors_) {
            for (auto& actor : meshActor->actors_) {
                const auto pvm = pv * actor.tform_.make_model_mat();
                actor.update_ubuf(pvm, queue_);
            }
        }

        const auto colorView = surface_pkg_.acquire_color_view();
        const wgpu::RenderPassColorAttachment colorAttachment{
            .view = colorView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = { 0.0, 0.0, 0.0, 0.5 },
        };
        const wgpu::RenderPassDepthStencilAttachment depthAttachment{
            .view = depth_view_,
            .depthLoadOp = wgpu::LoadOp::Clear,
            .depthStoreOp = wgpu::StoreOp::Discard,
            .depthClearValue = 1.0f,
        };
        const wgpu::RenderPassDescriptor renderPassDesc{
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
            .depthStencilAttachment = &depthAttachment,
        };

        const auto encoder = device_pkg_.create_cmd_encoder();
        {
            const auto pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetPipeline(pipeline_);

            for (const auto& meshActor : scene_.mesh_actors_) {
                pass.SetVertexBuffer(0, meshActor->mesh_.vtx_buf());
                pass.SetIndexBuffer(
                    meshActor->mesh_.idx_buf(), wgpu::IndexFormat::Uint16
                );

                for (const auto& actor : meshActor->actors_) {
                    pass.SetBindGroup(0, actor.bind_group());
                    pass.DrawIndexed(meshActor->mesh_.index_count());
                }
            }

            pass.End();
        }
        const auto commands = encoder.Finish();
        queue_.Submit(1, &commands);
        surface_pkg_.present();
        device_pkg_.process_events();
    }

    void Renderer::on_fbuf_resize(uint32_t new_width, uint32_t new_height) {
        surface_pkg_.on_fbuf_resize(new_width, new_height);
        depth_view_ = ::make_depth_view(
            device_pkg_.device_, surface_pkg_.width(), surface_pkg_.height()
        );
    }

    std::shared_ptr<Scene::MeshActor> Renderer::create_mesh(
        std::span<const Mesh::Vertex> vertices,
        std::span<const uint16_t> indices
    ) {
        auto meshActor = std::make_shared<Scene::MeshActor>();
        meshActor->mesh_.init(vertices, indices, queue_, device_pkg_.device_);
        scene_.mesh_actors_.push_back(meshActor);
        return meshActor;
    }

    Actor& Renderer::add_actor(
        Scene::MeshActor& meshActor, const glm::mat4& mvp
    ) {
        meshActor.actors_.emplace_back();
        meshActor.actors_.back().init(
            pipeline_.GetBindGroupLayout(0), device_pkg_.device_
        );
        meshActor.actors_.back().update_ubuf(mvp, queue_);
        return meshActor.actors_.back();
    }

}  // namespace practice
