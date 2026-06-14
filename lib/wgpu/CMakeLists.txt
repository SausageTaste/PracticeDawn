add_library(practice_wgpu
    src/device.cpp
    src/renderer.cpp
    src/transform.cpp
)
target_compile_features(practice_wgpu PUBLIC cxx_std_23)
target_include_directories(practice_wgpu PUBLIC include)
target_link_libraries(practice_wgpu PUBLIC
    dawn::webgpu_dawn
    glm::glm
)
