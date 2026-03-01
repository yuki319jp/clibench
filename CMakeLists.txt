cmake_minimum_required(VERSION 3.20)
project(CLIBench VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Find GLFW
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# Shader compilation
include(cmake/CompileShaders.cmake)

set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/compute.comp
    ${CMAKE_SOURCE_DIR}/shaders/triangle.vert
    ${CMAKE_SOURCE_DIR}/shaders/triangle.frag
    ${CMAKE_SOURCE_DIR}/shaders/texture_fill.frag
)

compile_shaders(SHADER_SOURCES COMPILED_SHADERS)

# Main executable
add_executable(clibench
    src/main.cpp
    src/vulkan_context.cpp
    src/cpu_benchmark.cpp
    src/compute_benchmark.cpp
    src/memory_benchmark.cpp
    src/graphics_benchmark.cpp
    src/stress_benchmark.cpp
    src/result_reporter.cpp
)

target_include_directories(clibench PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/shaders
)

target_link_libraries(clibench PRIVATE
    Vulkan::Vulkan
    glfw
)

# Platform-specific settings
if(APPLE)
    target_compile_definitions(clibench PRIVATE VK_USE_PLATFORM_MACOS_MVK)

    # Metal backend support
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 17)

    set(METAL_SOURCES
        src/metal_context.mm
        src/metal_compute_benchmark.mm
        src/metal_memory_benchmark.mm
        src/metal_graphics_benchmark.mm
    )

    target_sources(clibench PRIVATE ${METAL_SOURCES})

    set_source_files_properties(${METAL_SOURCES} PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )

    find_library(QUARTZCORE_LIBRARY QuartzCore)
    find_library(METAL_LIBRARY Metal)
    find_library(FOUNDATION_LIBRARY Foundation)

    target_link_libraries(clibench PRIVATE
        ${QUARTZCORE_LIBRARY}
        ${METAL_LIBRARY}
        ${FOUNDATION_LIBRARY}
    )

    target_compile_definitions(clibench PRIVATE CLIBENCH_HAS_METAL=1)
elseif(WIN32)
    target_compile_definitions(clibench PRIVATE VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
    target_compile_definitions(clibench PRIVATE VK_USE_PLATFORM_XCB_KHR)
endif()

target_compile_definitions(clibench PRIVATE
    CLIBENCH_VERSION="${PROJECT_VERSION}"
)

# Ensure shaders are compiled before the executable
add_dependencies(clibench compile_shaders)

install(TARGETS clibench RUNTIME DESTINATION bin)
