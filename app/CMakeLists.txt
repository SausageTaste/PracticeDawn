add_executable(PracticeDawn
    src/main.cpp
    src/sdl_window.cpp
)
target_compile_features(PracticeDawn PRIVATE cxx_std_23)
target_link_libraries(PracticeDawn PRIVATE dawn::webgpu_dawn SDL3::SDL3)
