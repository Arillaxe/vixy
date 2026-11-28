# Vixy

A voxel terrain engine written in C++20 on Vulkan 1.3, using GLFW and Dear ImGui. Terrain is generated and meshed on a background thread and drawn with a single indirect draw call.

## Requirements

- Windows with [MinGW-w64](https://www.mingw-w64.org/) (`g++` with C++20 support and `mingw32-make` on `PATH`)
- [Vulkan SDK](https://vulkan.lunarg.com/) 1.3 or newer; use a recent release, since it must include the `slangc` shader compiler
- [GLFW](https://www.glfw.org/) prebuilt binaries for MinGW-w64
- A GPU and driver with Vulkan 1.3 support

The other dependencies (GLM, Dear ImGui, stb, tinygltf, nlohmann/json, perlin noise) are vendored in `include/` and `src/imgui/`.

## Environment variables

| Variable     | Points to                                                             |
|--------------|-----------------------------------------------------------------------|
| `VULKAN_SDK` | The Vulkan SDK root (headers in `Include/`, libraries in `Lib/`, `slangc` in `bin/`) |
| `GLFW_PATH`  | The GLFW root (headers in `include/`, libraries in `lib-mingw-w64/`)  |

## Building

```
build.bat              debug build   -> main.exe
build.bat release      release build -> main-release.exe
mingw32-make clean     remove build artifacts
compile_shaders.bat    compile only the shaders (same as mingw32-make shaders)
mingw32-make test      build and run the unit tests -> tests.exe
```

Both builds compile the Slang shaders in `shaders/` to `slang.spv` and `cull.spv` in the repository root. The debug build enables asserts and Vulkan validation layers; the release build is optimized (`-O2`) and has both turned off.

The unit tests in `tests/` use [doctest](https://github.com/doctest/doctest) (vendored as `include/doctest.h`) and need no GPU or window.

## Running

Run `main.exe` or `main-release.exe`. The executable finds `slang.spv`, `cull.spv` and `textures/` in its own folder, so it can be launched from anywhere.

## Controls

| Input              | Action                 |
|--------------------|------------------------|
| W A S D            | Move                   |
| Space / Left Ctrl  | Move up / down         |
| Mouse              | Look around            |
| Left click         | Break a block          |
| Right click        | Place a block          |
| E                  | Toggle the cursor      |
| Esc                | Quit                   |
