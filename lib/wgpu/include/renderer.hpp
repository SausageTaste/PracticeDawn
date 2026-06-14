#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "device.hpp"
#include "transform.hpp"


namespace practice {

    class SurfacePackage {

    public:
        void init(
            const wgpu::SurfaceDescriptor& descriptor,
            const uint32_t width,
            const uint32_t height,
            const DevicePackage& dpkg
        );
        void present() const;
        void on_fbuf_resize(uint32_t new_width, uint32_t new_height);

        wgpu::TextureView acquire_color_view();

        auto& caps() const { return caps_; }
        auto width() const { return config_.width; }
        auto height() const { return config_.height; }

    private:
        wgpu::Surface surface_;
        wgpu::SurfaceCapabilities caps_;
        wgpu::SurfaceConfiguration config_;
        wgpu::SurfaceTexture tex_;
    };


    class Mesh {

    public:
        struct Vertex {
            float x, y, z, r, g, b, a;
        };

    public:
        void init(
            const std::span<const Vertex> vertices,
            const std::span<const uint16_t> indices,
            const wgpu::Queue& queue,
            const wgpu::Device& device
        );

        auto& vtx_buf() const { return vtx_buf_; }
        auto& idx_buf() const { return idx_buf_; }
        auto index_count() const { return index_count_; }

    private:
        wgpu::Buffer vtx_buf_;
        wgpu::Buffer idx_buf_;
        uint32_t index_count_ = 0;
    };


    class Actor {

    public:
        void init(
            const wgpu::BindGroupLayout& group_layout,
            const wgpu::Device& device
        );

        void update_ubuf(const glm::mat4& mvp, const wgpu::Queue& queue);

        auto& bind_group() const { return bind_group_; }

    private:
        wgpu::Buffer ubuf_;
        wgpu::BindGroup bind_group_;
    };


    class Scene {

    public:
        struct MeshActor {
            Mesh mesh_;
            std::deque<Actor> actors_;
        };

        std::vector<std::shared_ptr<MeshActor>> mesh_actors_;
        TransformQuat camera_view_;
    };


    class Renderer {

    public:
        void init();

        void create_surface(
            const wgpu::SurfaceDescriptor& descriptor,
            uint32_t width,
            uint32_t height
        );

        void create_render_pass(const std::filesystem::path& asset_dir);

        void tick();

        void on_fbuf_resize(uint32_t new_width, uint32_t new_height);

        std::shared_ptr<Scene::MeshActor> create_mesh(
            std::span<const Mesh::Vertex> vertices,
            std::span<const uint16_t> indices
        );

        Actor& add_actor(Scene::MeshActor& meshActor, const glm::mat4& mvp);
        void update_actor(Actor& actor, const glm::mat4& mvp);

        auto& scene() { return scene_; }

    private:
        DevicePackage device_pkg_;
        SurfacePackage surface_pkg_;
        Scene scene_;

        wgpu::Queue queue_;
        wgpu::TextureView depth_view_;

        wgpu::RenderPipeline pipeline_;
    };

}  // namespace practice
