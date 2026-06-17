add_executable(PracticeDawn
    src/cam_ctrl.cpp
    src/main.cpp
    src/sdl_window.cpp
)
target_link_libraries(PracticeDawn PRIVATE
    SDL3::SDL3
    glm::glm
    practice_wgpu
)
target_compile_definitions(PracticeDawn PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)

if (WIN32)
    # Dawn loads DXC dynamically, so vcpkg's app-local dependency copying does
    # not detect these DLLs automatically.
    add_custom_command(TARGET PracticeDawn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Microsoft::DirectXShaderCompiler>"
            "$<TARGET_FILE_DIR:PracticeDawn>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Microsoft::DXIL>"
            "$<TARGET_FILE_DIR:PracticeDawn>"
        VERBATIM
    )
endif()

if (EMSCRIPTEN)
    set_target_properties(PracticeDawn PROPERTIES SUFFIX ".html")
    target_link_options(PracticeDawn PRIVATE
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/assets@/assets"
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sEXIT_RUNTIME=0"
    )
endif()
