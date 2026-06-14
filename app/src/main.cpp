#include <array>
#include <cmath>
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

    constexpr std::array<practice::Mesh::Vertex, 24> kVertices{ {
        // Front  (+Z) - red
        { -0.5f, -0.5f, 0.5f, 1, 0, 0, 1 },
        { 0.5f, -0.5f, 0.5f, 1, 0, 0, 1 },
        { 0.5f, 0.5f, 0.5f, 1, 0, 0, 1 },
        { -0.5f, 0.5f, 0.5f, 1, 0, 0, 1 },
        // Back   (-Z) - green
        { 0.5f, -0.5f, -0.5f, 0, 1, 0, 1 },
        { -0.5f, -0.5f, -0.5f, 0, 1, 0, 1 },
        { -0.5f, 0.5f, -0.5f, 0, 1, 0, 1 },
        { 0.5f, 0.5f, -0.5f, 0, 1, 0, 1 },
        // Right  (+X) - blue
        { 0.5f, -0.5f, 0.5f, 0, 0, 1, 1 },
        { 0.5f, -0.5f, -0.5f, 0, 0, 1, 1 },
        { 0.5f, 0.5f, -0.5f, 0, 0, 1, 1 },
        { 0.5f, 0.5f, 0.5f, 0, 0, 1, 1 },
        // Left   (-X) - yellow
        { -0.5f, -0.5f, -0.5f, 1, 1, 0, 1 },
        { -0.5f, -0.5f, 0.5f, 1, 1, 0, 1 },
        { -0.5f, 0.5f, 0.5f, 1, 1, 0, 1 },
        { -0.5f, 0.5f, -0.5f, 1, 1, 0, 1 },
        // Top    (+Y) - cyan
        { -0.5f, 0.5f, 0.5f, 0, 1, 1, 1 },
        { 0.5f, 0.5f, 0.5f, 0, 1, 1, 1 },
        { 0.5f, 0.5f, -0.5f, 0, 1, 1, 1 },
        { -0.5f, 0.5f, -0.5f, 0, 1, 1, 1 },
        // Bottom (-Y) - magenta
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
    auto cube_mesh = wgpu_.create_mesh(kVertices, kIndices);
    auto& left_cube = wgpu_.add_actor(*cube_mesh, glm::mat4(1.0f));
    auto& right_cube = wgpu_.add_actor(*cube_mesh, glm::mat4(1.0f));

    auto& scene = wgpu_.scene();
    auto& camera_view = scene.camera_view_;
    camera_view.set_pos(0, 0, 4);

    while (true) {
        while (auto event = window.poll_event()) {
            if (event->type == SDL_EVENT_QUIT)
                return EXIT_SUCCESS;
            if (event->type == SDL_EVENT_WINDOW_RESIZED) {
                wgpu_.on_fbuf_resize(event->window.data1, event->window.data2);
            }
        }

        // Compute and upload MVP
        const double t = SDL_GetTicks() / 1000.0;
        const double aspect = static_cast<double>(window.width()) /
                              static_cast<double>(window.height());
        const auto proj = glm::perspectiveRH_ZO(
            glm::radians(60.0), aspect, 0.1, 100.0
        );
        const auto view = camera_view.make_view_mat();

        const auto left_model =
            glm::translate(glm::dmat4(1.0), glm::dvec3(-0.9, 0.0, 0.0)) *
            glm::rotate(glm::dmat4(1.0), t, glm::dvec3(0, 1, 0)) *
            glm::rotate(glm::dmat4(1.0), t * 0.5, glm::dvec3(1, 0, 0));

        const auto right_model =
            glm::translate(
                glm::dmat4(1.0), glm::dvec3(0.9, 0.35 * std::sin(t * 1.7), 0.0)
            ) *
            glm::rotate(glm::dmat4(1.0), -t * 1.4, glm::dvec3(1, 1, 0)) *
            glm::scale(glm::dmat4(1.0), glm::dvec3(0.7));
        wgpu_.update_actor(left_cube, proj * view * left_model);
        wgpu_.update_actor(right_cube, proj * view * right_model);
        wgpu_.tick();
    }

    return EXIT_SUCCESS;
}
