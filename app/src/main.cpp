#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(__APPLE__)
    #include <SDL3/SDL_metal.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

#include "device.hpp"
#include "sdl_window.hpp"


namespace {

    wgpu::Surface createSurface(wgpu::Instance instance, SDL_Window* window) {
#if defined(__APPLE__)
        SDL_MetalView view = SDL_Metal_CreateView(window);
        wgpu::SurfaceSourceMetalLayer src{};
        src.layer = SDL_Metal_GetLayer(view);
        wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
        return instance.CreateSurface(&desc);
#elif defined(_WIN32)
        wgpu::SurfaceSourceWindowsHWND src{};
        src.hinstance = GetModuleHandle(nullptr);
        src.hwnd = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr
        );
        wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
        return instance.CreateSurface(&desc);
#else
        wgpu::SurfaceSourceXlibWindow src{};
        src.display = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_X11_DISPLAY_POINTER,
            nullptr
        );
        src.window = static_cast<uint64_t>(SDL_GetNumberProperty(
            SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_X11_WINDOW_NUMBER,
            0
        ));
        wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
        return instance.CreateSurface(&desc);
#endif
    }

    std::filesystem::path find_assets_dir() {
        std::filesystem::path path = std::filesystem::current_path();
        while (path.has_parent_path()) {
            if (std::filesystem::exists(path / "assets"))
                return path / "assets";
            path = path.parent_path();
        }
        throw std::runtime_error("Failed to find assets directory!");
    }

    // ---- Cube geometry: 24 vertices (4 per face), 36 indices ----

    struct Vertex {
        float x, y, z, r, g, b, a;
    };

    constexpr std::array<Vertex, 24> kVertices{ {
        // Front  (+Z) — red
        { -0.5f, -0.5f, 0.5f, 1, 0, 0, 1 },
        { 0.5f, -0.5f, 0.5f, 1, 0, 0, 1 },
        { 0.5f, 0.5f, 0.5f, 1, 0, 0, 1 },
        { -0.5f, 0.5f, 0.5f, 1, 0, 0, 1 },
        // Back   (-Z) — green
        { 0.5f, -0.5f, -0.5f, 0, 1, 0, 1 },
        { -0.5f, -0.5f, -0.5f, 0, 1, 0, 1 },
        { -0.5f, 0.5f, -0.5f, 0, 1, 0, 1 },
        { 0.5f, 0.5f, -0.5f, 0, 1, 0, 1 },
        // Right  (+X) — blue
        { 0.5f, -0.5f, 0.5f, 0, 0, 1, 1 },
        { 0.5f, -0.5f, -0.5f, 0, 0, 1, 1 },
        { 0.5f, 0.5f, -0.5f, 0, 0, 1, 1 },
        { 0.5f, 0.5f, 0.5f, 0, 0, 1, 1 },
        // Left   (-X) — yellow
        { -0.5f, -0.5f, -0.5f, 1, 1, 0, 1 },
        { -0.5f, -0.5f, 0.5f, 1, 1, 0, 1 },
        { -0.5f, 0.5f, 0.5f, 1, 1, 0, 1 },
        { -0.5f, 0.5f, -0.5f, 1, 1, 0, 1 },
        // Top    (+Y) — cyan
        { -0.5f, 0.5f, 0.5f, 0, 1, 1, 1 },
        { 0.5f, 0.5f, 0.5f, 0, 1, 1, 1 },
        { 0.5f, 0.5f, -0.5f, 0, 1, 1, 1 },
        { -0.5f, 0.5f, -0.5f, 0, 1, 1, 1 },
        // Bottom (-Y) — magenta
        { -0.5f, -0.5f, -0.5f, 1, 0, 1, 1 },
        { 0.5f, -0.5f, -0.5f, 1, 0, 1, 1 },
        { 0.5f, -0.5f, 0.5f, 1, 0, 1, 1 },
        { -0.5f, -0.5f, 0.5f, 1, 0, 1, 1 },
    } };

    constexpr std::array<uint16_t, 36> kIndices{ {
        0,  1,  2,  0,  2,  3,   // front
        4,  5,  6,  4,  6,  7,   // back
        8,  9,  10, 8,  10, 11,  // right
        12, 13, 14, 12, 14, 15,  // left
        16, 17, 18, 16, 18, 19,  // top
        20, 21, 22, 20, 22, 23,  // bottom
    } };

    wgpu::TextureView makeDepthView(
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


int main(int argc, char* argv[]) {
    practice::Device wgpu_;
    wgpu_.init();
    const auto& device = wgpu_.device_;
    const auto queue = device.GetQueue();

    practice::WindowSDL3 window(1280, 720, "PracticeDawn");
    const auto surface = ::createSurface(wgpu_.instance_, window.get());
    const auto caps = wgpu_.get_surface_caps(surface);

    wgpu::SurfaceConfiguration config{
        .device = device,
        .format = caps.formats[0],
        .width = window.width(),
        .height = window.height(),
    };
    surface.Configure(&config);

    // Shader
    const auto shader_path = ::find_assets_dir() / "shaders" / "cube.wgsl";
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
    const wgpu::ShaderModuleDescriptor shaderDesc{ .nextInChain = &wgslSource };
    const auto shader = device.CreateShaderModule(&shaderDesc);

    // Geometry buffers
    constexpr uint64_t kVbSize = kVertices.size() * sizeof(Vertex);
    const wgpu::BufferDescriptor vbDesc{
        .usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst,
        .size = kVbSize,
    };
    const auto vertexBuffer = device.CreateBuffer(&vbDesc);
    queue.WriteBuffer(vertexBuffer, 0, kVertices.data(), kVbSize);

    constexpr uint64_t kIbSize = kIndices.size() * sizeof(uint16_t);
    const wgpu::BufferDescriptor ibDesc{
        .usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst,
        .size = kIbSize,
    };
    const auto indexBuffer = device.CreateBuffer(&ibDesc);
    queue.WriteBuffer(indexBuffer, 0, kIndices.data(), kIbSize);

    // MVP uniform buffer
    const wgpu::BufferDescriptor ubDesc{
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(glm::mat4),
    };
    const auto uniformBuffer = device.CreateBuffer(&ubDesc);

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

    // Bind group
    const wgpu::BindGroupEntry bgEntry{
        .binding = 0,
        .buffer = uniformBuffer,
        .size = sizeof(glm::mat4),
    };
    const wgpu::BindGroupDescriptor bgDesc{
        .layout = bindGroupLayout,
        .entryCount = 1,
        .entries = &bgEntry,
    };
    const auto bindGroup = device.CreateBindGroup(&bgDesc);

    // Render pipeline
    constexpr wgpu::TextureFormat kDepthFormat =
        wgpu::TextureFormat::Depth24Plus;
    auto depthView = ::makeDepthView(device, config.width, config.height);

    const std::array<wgpu::VertexAttribute, 2> vertexAttribs{ {
        { .format = wgpu::VertexFormat::Float32x3,
          .offset = 0,
          .shaderLocation = 0 },
        { .format = wgpu::VertexFormat::Float32x4,
          .offset = 12,
          .shaderLocation = 1 },
    } };
    const wgpu::VertexBufferLayout vertexLayout{
        .arrayStride = sizeof(Vertex),
        .attributeCount = vertexAttribs.size(),
        .attributes = vertexAttribs.data(),
    };
    const wgpu::ColorTargetState colorTarget{ .format = caps.formats[0] };
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
    const auto pipeline = device.CreateRenderPipeline(&pipelineDesc);

    while (true) {
        while (auto event = window.poll_event()) {
            if (event->type == SDL_EVENT_QUIT)
                return EXIT_SUCCESS;
            if (event->type == SDL_EVENT_WINDOW_RESIZED) {
                config.width = static_cast<uint32_t>(event->window.data1);
                config.height = static_cast<uint32_t>(event->window.data2);
                surface.Configure(&config);
                depthView = ::makeDepthView(
                    device, config.width, config.height
                );
            }
        }

        // Compute and upload MVP
        const float t = SDL_GetTicks() / 1000.0f;
        const float aspect = static_cast<float>(config.width) /
                             static_cast<float>(config.height);
        const auto proj = glm::perspective(
            glm::radians(60.0f), aspect, 0.1f, 100.0f
        );
        const auto view = glm::translate(
            glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f)
        );
        const auto model = glm::rotate(glm::mat4(1.0f), t, glm::vec3(0, 1, 0)) *
                           glm::rotate(
                               glm::mat4(1.0f), t * 0.5f, glm::vec3(1, 0, 0)
                           );
        const auto mvp = proj * view * model;
        queue.WriteBuffer(
            uniformBuffer, 0, glm::value_ptr(mvp), sizeof(glm::mat4)
        );

        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);
        if (!surfaceTexture.texture)
            continue;

        const auto colorView = surfaceTexture.texture.CreateView();
        const wgpu::RenderPassColorAttachment colorAttachment{
            .view = colorView,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = { 0.1, 0.1, 0.1, 1.0 },
        };
        const wgpu::RenderPassDepthStencilAttachment depthAttachment{
            .view = depthView,
            .depthLoadOp = wgpu::LoadOp::Clear,
            .depthStoreOp = wgpu::StoreOp::Discard,
            .depthClearValue = 1.0f,
        };
        const wgpu::RenderPassDescriptor renderPassDesc{
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
            .depthStencilAttachment = &depthAttachment,
        };

        const auto encoder = wgpu_.create_cmd_encoder();
        {
            const auto pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, bindGroup);
            pass.SetVertexBuffer(0, vertexBuffer);
            pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint16);
            pass.DrawIndexed(static_cast<uint32_t>(kIndices.size()));
            pass.End();
        }
        const auto commands = encoder.Finish();
        queue.Submit(1, &commands);
        surface.Present();
        wgpu_.process_events();
    }

    return EXIT_SUCCESS;
}
