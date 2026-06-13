add_executable(PracticeDawn
    src/main.cpp
    src/sdl_window.cpp
)
target_link_libraries(PracticeDawn PRIVATE
    SDL3::SDL3
    glm::glm
    practice_wgpu
)
target_compile_definitions(PracticeDawn PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
