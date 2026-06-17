#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

#if defined(__EMSCRIPTEN__)
    #include <emscripten/emscripten.h>
    #include <emscripten/html5.h>
#elif defined(__APPLE__)
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
#if defined(__EMSCRIPTEN__)
        return "/assets";
#else
        std::filesystem::path path = std::filesystem::current_path();
        while (path.has_parent_path()) {
            if (std::filesystem::exists(path / "assets"))
                return path / "assets";
            path = path.parent_path();
        }
        throw std::runtime_error("Failed to find assets directory!");
#endif
    }

    class App {

    public:
        void start_native() {
            renderer_.init();
            finish_init();
        }

        void start_web() {
            renderer_.init_async([this]() {
                finish_init();
#if defined(__EMSCRIPTEN__)
                emscripten_request_animation_frame_loop(&App::web_frame, this);
#endif
            });
        }

        bool tick_frame(double dt) {
            if (!poll_events())
                return false;

            auto& camera_view = renderer_.scene().camera_view_;
            practice::update_camera(camera_view, camera_input_, dt);

            left_cube_->tform_.rotate(dt, glm::dvec3(0, 1, 0));
            right_cube_->tform_.rotate(dt, glm::dvec3(1, 1, 0));

            renderer_.tick();
            return true;
        }

        bool tick_frame_from_clock() {
            const auto now = SDL_GetTicks();
            const auto dt = (now - last_tick_) / 1000.0;
            last_tick_ = now;
            return tick_frame(dt);
        }

    private:
        bool poll_events() {
            while (const auto* event = window_->poll_event()) {
                if (event->type == SDL_EVENT_QUIT)
                    return false;
                if (event->type == SDL_EVENT_WINDOW_RESIZED) {
                    renderer_.on_fbuf_resize(
                        event->window.data1, event->window.data2
                    );
                }
                if (event->type == SDL_EVENT_KEY_DOWN ||
                    event->type == SDL_EVENT_KEY_UP) {
                    if (event->key.key == SDLK_ESCAPE && event->key.down)
                        return false;
                    set_key(camera_input_, event->key.key, event->key.down);
                }
                if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                    event->button.button == SDL_BUTTON_RIGHT) {
                    camera_input_.mouse_look = true;
                    SDL_SetWindowRelativeMouseMode(window_->get(), true);
                }
                if (event->type == SDL_EVENT_MOUSE_BUTTON_UP &&
                    event->button.button == SDL_BUTTON_RIGHT) {
                    camera_input_.mouse_look = false;
                    SDL_SetWindowRelativeMouseMode(window_->get(), false);
                }
                if (event->type == SDL_EVENT_MOUSE_MOTION &&
                    camera_input_.mouse_look) {
                    constexpr double kMouseSensitivity = 0.0025;
                    constexpr double kPitchLimit = kPi * 0.49;
                    camera_input_.yaw += event->motion.xrel * kMouseSensitivity;
                    camera_input_.pitch = std::clamp(
                        camera_input_.pitch -
                            event->motion.yrel * kMouseSensitivity,
                        -kPitchLimit,
                        kPitchLimit
                    );
                }
            }
            return true;
        }

        void finish_init() {
            window_ = std::make_unique<practice::WindowSDL3>(
                1280, 720, "PracticeDawn"
            );
            create_surface();

            renderer_.create_render_pass(::find_assets_dir());
            cube_mesh_ = renderer_.create_mesh(kVertices, kIndices);
            left_cube_ = &renderer_.add_actor(*cube_mesh_, glm::mat4(1.0f));
            right_cube_ = &renderer_.add_actor(*cube_mesh_, glm::mat4(1.0f));

            left_cube_->tform_.set_pos(-0.9, 0, 0);
            right_cube_->tform_.set_pos(0.9, 0, 0);

            auto& camera_view = renderer_.scene().camera_view_;
            camera_view.set_pos(0, 0, 4);
            practice::update_camera(camera_view, camera_input_, 0.0);

            last_tick_ = SDL_GetTicks();
        }

        void create_surface() {
#if defined(__EMSCRIPTEN__)
            wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector src{};
            src.selector = "#canvas";
            wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
#elif defined(__APPLE__)
            SDL_MetalView view = SDL_Metal_CreateView(window_->get());
            wgpu::SurfaceSourceMetalLayer src{};
            src.layer = SDL_Metal_GetLayer(view);
            wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
#elif defined(_WIN32)
            wgpu::SurfaceSourceWindowsHWND src{};
            src.hinstance = GetModuleHandle(nullptr);
            src.hwnd = SDL_GetPointerProperty(
                SDL_GetWindowProperties(window_->get()),
                SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                nullptr
            );
            wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
#else
            wgpu::SurfaceSourceXlibWindow src{};
            src.display = SDL_GetPointerProperty(
                SDL_GetWindowProperties(window_->get()),
                SDL_PROP_WINDOW_X11_DISPLAY_POINTER,
                nullptr
            );
            src.window = static_cast<uint64_t>(SDL_GetNumberProperty(
                SDL_GetWindowProperties(window_->get()),
                SDL_PROP_WINDOW_X11_WINDOW_NUMBER,
                0
            ));
            wgpu::SurfaceDescriptor desc{ .nextInChain = &src };
#endif
            renderer_.create_surface(desc, window_->width(), window_->height());
        }

#if defined(__EMSCRIPTEN__)
        static EM_BOOL web_frame(double time, void* user_data) {
            auto* app = static_cast<App*>(user_data);
            if (app->last_frame_time_ == 0.0)
                app->last_frame_time_ = time;
            const auto dt = (time - app->last_frame_time_) / 1000.0;
            app->last_frame_time_ = time;
            return app->tick_frame(dt) ? EM_TRUE : EM_FALSE;
        }
#endif

        practice::Renderer renderer_;
        std::unique_ptr<practice::WindowSDL3> window_;
        std::shared_ptr<practice::Scene::MeshActor> cube_mesh_;
        practice::Actor* left_cube_ = nullptr;
        practice::Actor* right_cube_ = nullptr;
        practice::CameraInput camera_input_;
        uint64_t last_tick_ = 0;
        double last_frame_time_ = 0.0;
    };

}  // namespace


int main(int argc, char* argv[]) {
#if defined(__EMSCRIPTEN__)
    auto* app = new App();
    app->start_web();
    return EXIT_SUCCESS;
#else
    App app;
    app.start_native();
    while (app.tick_frame_from_clock()) {
    }
    return EXIT_SUCCESS;
#endif
}
