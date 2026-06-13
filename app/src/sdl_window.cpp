#include "sdl_window.hpp"

#include <format>
#include <stdexcept>


namespace practice {

    WindowSDL3::WindowSDL3(int width, int height, const char* title) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            throw std::runtime_error(
                std::format("Failed to initialize SDL: {}", SDL_GetError())
            );
        }

        width_ = width;
        height_ = height;
        window_ = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_TRANSPARENT);
        if (!window_) {
            throw std::runtime_error(
                std::format("Failed to create SDL window: {}", SDL_GetError())
            );
        }
    }

    WindowSDL3::~WindowSDL3() {
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    const SDL_Event* WindowSDL3::poll_event() {
        if (SDL_PollEvent(&event_))
            return &event_;
        else
            return nullptr;
    }

}  // namespace practice
