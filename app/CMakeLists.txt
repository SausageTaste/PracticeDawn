add_executable(PracticeDawn
    src/main.cpp
    src/sdl_window.cpp
)
target_link_libraries(PracticeDawn PRIVATE
    SDL3::SDL3
    practice_wgpu
)
