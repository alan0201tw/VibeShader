include(FetchContent)
# TinyObjLoader for mesh import
FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG        release
)
FetchContent_MakeAvailable(tinyobjloader)
cmake_minimum_required(VERSION 3.28)
project(Playground
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler warnings ────────────────────────────────────────────────────────
function(apply_project_warnings target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target_name} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT WIN32)
      target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined)
      target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

# ── Dependencies via FetchContent ────────────────────────────────────────────
include(FetchContent)

# Vulkan SDK (must be installed; find via CMake)
find_package(Vulkan REQUIRED COMPONENTS glslc)

# GLFW for window + input
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM for math (column-major, right-handed)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# Google Test
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── Shader compilation ───────────────────────────────────────────────────────
include(cmake/CompileShaders.cmake)

file(GLOB_RECURSE SHADER_SOURCES
  "${CMAKE_SOURCE_DIR}/shaders/*.vert"
  "${CMAKE_SOURCE_DIR}/shaders/*.frag"
  "${CMAKE_SOURCE_DIR}/shaders/*.comp"
  "${CMAKE_SOURCE_DIR}/shaders/*.geom"
  "${CMAKE_SOURCE_DIR}/shaders/*.tesc"
  "${CMAKE_SOURCE_DIR}/shaders/*.tese"
)

compile_shaders(
  TARGET playground_shaders
  SOURCES ${SHADER_SOURCES}
  OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders"
)

# ── Main executable ──────────────────────────────────────────────────────────
add_executable(playground
  src/main.cc
  src/graphics/vulkan_context.cc
  src/graphics/swapchain.cc
  src/graphics/pipeline.cc
  src/graphics/shader_module.cc
  src/graphics/renderer.cc
  src/graphics/mesh_loader.cc
  src/graphics/buffer_util.cc
)

target_include_directories(playground PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(playground PRIVATE
  Vulkan::Vulkan
  glfw
  glm::glm
  tinyobjloader
)
apply_project_warnings(playground)

add_dependencies(playground playground_shaders)

# Pass shader output directory as a compile definition
target_compile_definitions(playground PRIVATE
  SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
