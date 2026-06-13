#include <cstdlib>
#include <iostream>
#include <vector>

#include <webgpu/webgpu_cpp.h>

#if defined(__APPLE__)
    #include <SDL3/SDL_metal.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

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
    auto instance = []() {
        const std::vector<wgpu::InstanceFeatureName> required_features{
            wgpu::InstanceFeatureName::TimedWaitAny,
        };

        const wgpu::InstanceDescriptor instanceDescriptor{
            .requiredFeatureCount = required_features.size(),
            .requiredFeatures = required_features.data()
        };

        auto instance = wgpu::CreateInstance(&instanceDescriptor);
        if (!instance) {
            throw std::runtime_error("Failed to create WebGPU instance!");
        }
        return instance;
    }();

    auto adapter = [&instance]() {
        wgpu::RequestAdapterOptions adapterOptions{};
        wgpu::Adapter adapter;
        instance.WaitAny(
            instance.RequestAdapter(
                &adapterOptions,
                wgpu::CallbackMode::WaitAnyOnly,
                [&adapter](
                    wgpu::RequestAdapterStatus status,
                    wgpu::Adapter a,
                    wgpu::StringView msg
                ) {
                    if (status == wgpu::RequestAdapterStatus::Success)
                        adapter = a;
                    else
                        throw std::runtime_error(
                            "RequestAdapter failed: " +
                            std::string(msg.data, msg.length)
                        );
                }
            ),
            UINT64_MAX
        );
        if (!adapter)
            throw std::runtime_error("Failed to get WebGPU adapter!");
        return adapter;
    }();

    auto device = [&adapter, &instance]() {
        wgpu::Device device;
        wgpu::DeviceDescriptor deviceDesc{};
        instance.WaitAny(
            adapter.RequestDevice(
                &deviceDesc,
                wgpu::CallbackMode::WaitAnyOnly,
                [&device](
                    wgpu::RequestDeviceStatus status,
                    wgpu::Device d,
                    wgpu::StringView msg
                ) {
                    if (status == wgpu::RequestDeviceStatus::Success)
                        device = d;
                    else
                        throw std::runtime_error(
                            "RequestDevice failed: " +
                            std::string(msg.data, msg.length)
                        );
                }
            ),
            UINT64_MAX
        );
        if (!device)
            throw std::runtime_error("Failed to get WebGPU device!");

        return device;
    }();

    practice::WindowSDL3 window(1280, 720, "PracticeDawn");
    auto surface = ::createSurface(instance, window.get());

    const auto caps = [&surface, &adapter]() {
        wgpu::SurfaceCapabilities caps;
        surface.GetCapabilities(adapter, &caps);
        if (caps.formatCount == 0) {
            throw std::runtime_error("Surface has no supported formats!");
        }
        return caps;
    }();

    wgpu::SurfaceConfiguration config{
        .device = device,
        .format = caps.formats[0],
        .width = window.width(),
        .height = window.height(),
    };
    surface.Configure(&config);

    auto queue = device.GetQueue();

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

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
        pass.End();
        wgpu::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);
        surface.Present();
        instance.ProcessEvents();
    }

    return EXIT_SUCCESS;
}
