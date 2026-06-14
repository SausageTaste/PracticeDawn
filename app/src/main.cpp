#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

#if defined(__APPLE__)
    #include <SDL3/SDL_metal.h>
#elif defined(_WIN32)
    #include <windows.h>
#endif

#include "renderer.hpp"
#include "sdl_window.hpp"


namespace {

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
    practice::Renderer wgpu_;
    wgpu_.init();

    practice::WindowSDL3 window(1280, 720, "PracticeDawn");
    {
#if defined(__APPLE__)
        SDL_MetalView view = SDL_Metal_CreateView(window.get());
        wgpu::SurfaceSourceMetalLayer src{};
        src.layer = SDL_Metal_GetLayer(view);
        wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
#elif defined(_WIN32)
        wgpu::SurfaceSourceWindowsHWND src{};
        src.hinstance = GetModuleHandle(nullptr);
        src.hwnd = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.get()),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr
        );
        wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
#else
        wgpu::SurfaceSourceXlibWindow src{};
        src.display = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.get()),
            SDL_PROP_WINDOW_X11_DISPLAY_POINTER,
            nullptr
        );
        src.window = static_cast<uint64_t>(SDL_GetNumberProperty(
            SDL_GetWindowProperties(window.get()),
            SDL_PROP_WINDOW_X11_WINDOW_NUMBER,
            0
        ));
        wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
#endif
        wgpu_.create_surface(desc, window.width(), window.height());
    }
    wgpu_.create_render_pass(::find_assets_dir());

    while (true) {
        while (auto event = window.poll_event()) {
            if (event->type == SDL_EVENT_QUIT)
                return EXIT_SUCCESS;
            if (event->type == SDL_EVENT_WINDOW_RESIZED) {
                wgpu_.on_fbuf_resize(event->window.data1, event->window.data2);
            }
        }

        // Compute and upload MVP
        const float t = SDL_GetTicks() / 1000.0f;
        const float aspect = static_cast<float>(window.width()) /
                             static_cast<float>(window.height());
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
        wgpu_.tick(mvp);
    }

    return EXIT_SUCCESS;
}
