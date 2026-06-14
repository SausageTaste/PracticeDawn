#include <algorithm>
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

#include "cam_ctrl.hpp"
#include "renderer.hpp"
#include "sdl_window.hpp"


namespace {

    constexpr double kPi = 3.14159265358979323846;

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

    left_cube.tform_.set_pos(-0.9, 0, 0);
    right_cube.tform_.set_pos(0.9, 0, 0);

    auto& scene = wgpu_.scene();
    auto& camera_view = scene.camera_view_;
    camera_view.set_pos(0, 0, 4);
    practice::CameraInput camera_input;
    practice::update_camera(camera_view, camera_input, 0.0);

    auto last_tick = SDL_GetTicks();

    while (true) {
        const auto dt = (SDL_GetTicks() - last_tick) / 1000.0;
        last_tick = SDL_GetTicks();

        while (auto event = window.poll_event()) {
            if (event->type == SDL_EVENT_QUIT)
                return EXIT_SUCCESS;
            if (event->type == SDL_EVENT_WINDOW_RESIZED) {
                wgpu_.on_fbuf_resize(event->window.data1, event->window.data2);
            }
            if (event->type == SDL_EVENT_KEY_DOWN ||
                event->type == SDL_EVENT_KEY_UP) {
                if (event->key.key == SDLK_ESCAPE && event->key.down)
                    return EXIT_SUCCESS;
                set_key(camera_input, event->key.key, event->key.down);
            }
            if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event->button.button == SDL_BUTTON_RIGHT) {
                camera_input.mouse_look = true;
                SDL_SetWindowRelativeMouseMode(window.get(), true);
            }
            if (event->type == SDL_EVENT_MOUSE_BUTTON_UP &&
                event->button.button == SDL_BUTTON_RIGHT) {
                camera_input.mouse_look = false;
                SDL_SetWindowRelativeMouseMode(window.get(), false);
            }
            if (event->type == SDL_EVENT_MOUSE_MOTION &&
                camera_input.mouse_look) {
                constexpr double kMouseSensitivity = 0.0025;
                constexpr double kPitchLimit = kPi * 0.49;
                camera_input.yaw += event->motion.xrel * kMouseSensitivity;
                camera_input.pitch = std::clamp(
                    camera_input.pitch - event->motion.yrel * kMouseSensitivity,
                    -kPitchLimit,
                    kPitchLimit
                );
            }
        }

        practice::update_camera(camera_view, camera_input, dt);

        left_cube.tform_.rotate(dt, glm::dvec3(0, 1, 0));
        right_cube.tform_.rotate(dt, glm::dvec3(1, 1, 0));

        wgpu_.tick();
    }

    return EXIT_SUCCESS;
}
