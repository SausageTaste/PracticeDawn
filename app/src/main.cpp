#include <cstdlib>
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
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
        pass.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
        surface.Present();
        wgpu_.process_events();
    }

    return EXIT_SUCCESS;
}
