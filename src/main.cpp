#include <cstdlib>
#include <iostream>
#include <vector>

#include <SDL3/SDL.h>
#include <webgpu/webgpu_cpp.h>

#if defined(__APPLE__)
    #include <SDL3/SDL_metal.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

static wgpu::Surface createSurface(
    wgpu::Instance instance, SDL_Window* window
) {
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
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0
    ));
    wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
    return instance.CreateSurface(&desc);
#endif
}

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

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }

    constexpr uint32_t kWidth = 800, kHeight = 600;
    SDL_Window* window = SDL_CreateWindow("PracticeDawn", kWidth, kHeight, 0);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }

    wgpu::Surface surface = createSurface(instance, window);
    if (!surface) {
        std::cerr << "Surface creation failed!\n";
        return EXIT_FAILURE;
    }

    wgpu::SurfaceCapabilities caps;
    surface.GetCapabilities(adapter, &caps);
    if (caps.formatCount == 0) {
        std::cerr << "No surface formats available!\n";
        return EXIT_FAILURE;
    }

    wgpu::SurfaceConfiguration config{
        .device = device,
        .format = caps.formats[0],
        .width = kWidth,
        .height = kHeight,
    };
    surface.Configure(&config);

    wgpu::Queue queue = device.GetQueue();

    SDL_Event event;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&event))
            if (event.type == SDL_EVENT_QUIT)
                running = false;

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

    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
