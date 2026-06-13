cmake_minimum_required(VERSION 3.16)

if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

project(PracticeDawn
    VERSION 0.1.0
    LANGUAGES CXX
)


find_package(Dawn CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

if (WIN32)
    find_package(directx-dxc CONFIG REQUIRED)
endif()


add_subdirectory(lib/wgpu)
add_subdirectory(app)
