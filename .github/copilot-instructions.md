# Project Guidelines

Modern C++23 (or later) project focused on Computer Graphics and Computer Animation.

## Code Style

Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) and [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).

### Naming

- Types and classes: `PascalCase` (`MeshRenderer`, `BoundingBox`)
- Functions and methods: `PascalCase` (`ComputeNormals()`, `GetWorldTransform()`)
- Variables and parameters: `snake_case` (`vertex_count`, `light_direction`)
- Class data members: `snake_case_` with trailing underscore (`transform_`, `vertex_buffer_`)
- Constants and compile-time values: `kPascalCase` (`kMaxLights`, `kDefaultFov`)
- Enumerators: `kPascalCase` (`kDiffuse`, `kSpecular`)
- Namespaces: `lowercase` (`graphics`, `math`, `animation`)
- File names: `snake_case.h` / `snake_case.cc`
- Header guards: `#pragma once`

### Formatting

- 2-space indentation, no tabs
- 80-column soft limit (allow up to 100 for math-heavy expressions)
- Opening brace on the same line
- `clang-format` with Google style as baseline

### Headers

- Use `#pragma once` for header guards
- Include order: related header, C system headers, C++ standard library, third-party, project headers — each group separated by a blank line
- Prefer forward declarations over includes where possible

## C++23 and Modern C++ Practices

- Prefer `std::expected` over exceptions for recoverable errors
- Use `std::optional` for values that may not exist
- Use `std::span` for non-owning contiguous ranges
- Use `std::format` / `std::print` instead of `printf` or `iostream` formatting
- Use `constexpr` and `consteval` aggressively for compile-time computation (especially math utilities)
- Use concepts and `requires` clauses to constrain templates
- Use structured bindings, `auto`, and range-based for loops idiomatically
- Use `std::unique_ptr` for single ownership, `std::shared_ptr` only when shared ownership is truly needed
- Use `std::array` over C-style arrays; `std::vector` for dynamic storage
- Use scoped enums (`enum class`) exclusively
- Mark single-argument constructors `explicit`
- Use `[[nodiscard]]` on functions where ignoring the return value is a bug
- Use modules (`import std;`) when the toolchain supports it; otherwise stick to `#include`

## Architecture — Graphics & Animation

### Math Conventions

- Right-handed coordinate system
- Column-major matrices (OpenGL convention unless project specifies otherwise)
- Radians for all internal angle representations; degrees only at UI boundaries
- Use a dedicated math library (e.g., GLM, Eigen, or a custom `math/` module)

### Resource and Memory Management

- RAII for all GPU and system resources (buffers, textures, shaders, framebuffers)
- Wrap raw API handles (`GLuint`, `VkBuffer`, etc.) in owning types with move semantics
- Separate resource creation from usage — prefer factory functions or builders
- Pool per-frame allocations; avoid allocating in hot loops

### Rendering Patterns

- Separate scene description from rendering (scene graph vs. render queue)
- Group draw calls by shared state (shader, material, texture) to minimize state changes
- Keep shader uniform updates batched; avoid per-draw uniform uploads when avoidable
- Abstract the graphics API behind a thin render backend interface for portability

### Animation

- Store transforms as separate translation, rotation (`quaternion`), scale components — compose matrices only when needed
- Use quaternion `slerp` for rotational interpolation
- Time values in seconds (`float` or `double`), delta-time passed explicitly — never rely on global timers
- Keyframe data and skeletal hierarchies should be data-driven (loaded from files, not hard-coded)

## Build and Test

```bash
# Configure (CMake ≥ 3.28, Ninja recommended)
cmake -B build -G Ninja -DCMAKE_CXX_STANDARD=23 -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

- Use **CMake** as the build system with a minimum of CMake 3.28
- Prefer `target_*` commands over global `include_directories` / `link_libraries`
- Use `FetchContent` or `vcpkg` for third-party dependencies
- Write tests with **Google Test** or **Catch2**; place tests under `tests/`
- Enable warnings: `-Wall -Wextra -Wpedantic -Werror` (MSVC: `/W4 /WX`)
- Enable sanitizers in debug builds: `-fsanitize=address,undefined`

## Conventions

- Prefer value semantics; pass small types by value, larger types by `const&` or `std::span`
- Hot-path code (inner render/simulation loops): minimize allocations, virtual calls, and cache misses — prefer SOA layouts for bulk data
- Shader source files: `.vert`, `.frag`, `.comp`, `.glsl` (or `.hlsl`)  — keep under `shaders/`
- Assets (models, textures, animations) live under `assets/`, never committed as large binaries without LFS
- GPU debugging: annotate command buffers and passes with debug labels/markers
- Avoid `using namespace` in headers; acceptable in `.cc` files at function scope for math namespaces
