#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

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

}  // namespace


int main(int argc, char* argv[]) {
    practice::Device wgpu_;
    wgpu_.init();

    practice::WindowSDL3 window(1280, 720, "PracticeDawn");
    const auto surface = ::createSurface(wgpu_.instance_, window.get());

    const auto& device = wgpu_.device_;
    const auto queue = device.GetQueue();
    const auto caps = wgpu_.get_surface_caps(surface);

    wgpu::SurfaceConfiguration config{
        .device = device,
        .format = caps.formats[0],
        .width = window.width(),
        .height = window.height(),
    };
    surface.Configure(&config);

    const auto asset_dir = ::find_assets_dir();
    const auto shader_path = asset_dir / "shaders" / "triangle.wgsl";
    std::ifstream shader_file(shader_path);
    if (!shader_file.is_open())
        throw std::runtime_error(
            "Failed to open shader file: " + shader_path.string()
        );
    const std::string shader_code(
        (std::istreambuf_iterator<char>(shader_file)),
        std::istreambuf_iterator<char>()
    );

    wgpu::ShaderSourceWGSL wgslSource{};
    wgslSource.code = shader_code.c_str();
    const wgpu::ShaderModuleDescriptor shaderDesc{ .nextInChain = &wgslSource };
    const auto shader = device.CreateShaderModule(&shaderDesc);

    const wgpu::ColorTargetState colorTarget{ .format = caps.formats[0] };
    const wgpu::FragmentState fragmentState{
        .module = shader,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = &colorTarget,
    };
    const wgpu::RenderPipelineDescriptor pipelineDesc{
        .vertex = { .module = shader, .entryPoint = "vs_main" },
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
            }
        }

        wgpu::SurfaceTexture surfaceTexture;
        surface.GetCurrentTexture(&surfaceTexture);
        if (!surfaceTexture.texture)
            continue;

        wgpu::TextureView view = surfaceTexture.texture.CreateView();
        wgpu::RenderPassColorAttachment colorAttachment{
            .view = view,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = { 0.0, 0.0, 1.0, 1.0 },
        };
        wgpu::RenderPassDescriptor renderPassDesc{
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
        };

        const auto encoder = wgpu_.create_cmd_encoder();
        {
            const auto pass = encoder.BeginRenderPass(&renderPassDesc);
            pass.SetPipeline(pipeline);
            pass.Draw(3);
            pass.End();
        }
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
        surface.Present();
        wgpu_.process_events();
    }

    return EXIT_SUCCESS;
}
