cmake_minimum_required(VERSION 3.16)

if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

project(PracticeDawn
    VERSION 0.1.0
    LANGUAGES CXX
)


if (EMSCRIPTEN)
    include(FetchContent)

    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glm)

    add_library(dawn::webgpu_dawn INTERFACE IMPORTED)
    target_compile_options(dawn::webgpu_dawn INTERFACE --use-port=emdawnwebgpu)
    target_link_options(dawn::webgpu_dawn INTERFACE --use-port=emdawnwebgpu)

    add_library(SDL3::SDL3 INTERFACE IMPORTED)
    target_compile_options(SDL3::SDL3 INTERFACE --use-port=sdl3)
    target_link_options(SDL3::SDL3 INTERFACE --use-port=sdl3)
else()
    find_package(Dawn CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(SDL3 CONFIG REQUIRED)

    if (WIN32)
        find_package(directx-dxc CONFIG REQUIRED)
    endif()
endif()


add_subdirectory(lib/wgpu)
add_subdirectory(app)
