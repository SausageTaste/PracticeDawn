#pragma once

#include <cstdint>

#include <SDL3/SDL.h>


namespace practice {

    class WindowSDL3 {

    public:
        WindowSDL3(int width, int height, const char* title);
        ~WindowSDL3();

        const SDL_Event* poll_event();

        SDL_Window* get() const { return window_; }
        uint32_t width() const { return width_; }
        uint32_t height() const { return height_; }

    private:
        SDL_Window* window_ = nullptr;
        SDL_Event event_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
    };

}  // namespace practice
