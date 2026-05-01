# Building noodles

`noodles` uses CMake (3.14+) and a C++17 compiler. Third-party dependencies
are fetched automatically by `FetchContent` if they are not found on the
system.

## Requirements

| Dependency   | Minimum version                          | How it is obtained                       |
|--------------|------------------------------------------|------------------------------------------|
| CMake        | 3.14                                     | system                                   |
| C++ compiler | C++17 (GCC 9+, Clang 10+, MSVC 2019+)    | system                                   |
| OpenGL       | 3.3 core                                 | system (`find_package(OpenGL)`)          |
| GLEW         | 2.2.0                                    | system, else `FetchContent` from upstream|
| stb_image    | pinned commit                            | `FetchContent`                           |
| RapidJSON    | 1.1.0                                    | system, else `FetchContent`              |
| GoogleTest   | any                                      | system (optional, only for tests)        |

On Windows, install OpenGL via your graphics driver. On Linux, install the
Mesa development headers (`libgl-dev`, `libglu1-mesa-dev`). On macOS, the
system OpenGL framework is used; note that Apple has deprecated OpenGL but
3.3 core is still functional.

## Configure and build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

## Build options

| Option                 | Default                | Effect                                |
|------------------------|------------------------|---------------------------------------|
| `CMAKE_BUILD_TYPE`     | (none)                 | Set to `Release` or `Debug`           |
| `BUILD_SHARED_LIBS`    | ON (lib is `SHARED`)   | Built as a shared library             |

Tests are configured automatically when GoogleTest is found
(`find_package(GTest)`). To force-skip tests, build without GoogleTest
installed.

## Running tests

```bash
ctest --test-dir build --output-on-failure
```

## Installing

```bash
cmake --install build --prefix /your/install/prefix
```

This installs:

```text
<prefix>/bin/                       # noodles shared library (Windows DLL)
<prefix>/lib/                       # noodles import / shared library
<prefix>/include/noodles/core/      # public headers
<prefix>/include/noodles/render/
<prefix>/include/noodles/spatial/
<prefix>/include/noodles/undo/
<prefix>/share/noodles/assets/      # bundled fonts and shaders
```

## Consuming `noodles` from another CMake project

```cmake
find_package(noodles REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE noodles)
```

The `noodles` target propagates its public include directories and a link
dependency on `OpenGL::GL` and `GLEW::GLEW`.

## Loading bundled assets at runtime

Renderers expect to be told where to find the bundled fonts and shaders.
After installation they live under `<prefix>/share/noodles/assets/`. Pass
that path to `FontAtlas` and `ShaderLibrary` in your application's
initialization code, or copy the `assets/` directory next to your
executable.

## Platform notes

### Windows

Build with the Visual Studio generator or Ninja. The library defines
`NOODLES_EXPORTS` when built and `NOODLES_API` resolves to `__declspec(...)`
for shared linkage. Define `NOODLES_STATIC` when consuming a static build.

### Linux / macOS

The library uses default `-fvisibility=hidden`-style export decoration. No
extra flags are required for consumers.

### Headless / CI builds

The library itself does not create a GL context. For automated tests that
need GL, use OSMesa, EGL pbuffers, or a virtual framebuffer (`xvfb-run`).
The headless renderer used internally at Meta is **not** included in this
repository because it depends on internal infrastructure.
