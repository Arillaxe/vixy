# Vixy

A voxel terrain engine in C++20 on Vulkan 1.3 (vulkan-hpp RAII), using GLFW and Dear ImGui. It builds on Windows with MinGW-w64 g++.

## Build and run

- **Build:** `build.bat` (runs `mingw32-make -j%NUMBER_OF_PROCESSORS%` plus any arguments, e.g. `build.bat release`), or `mingw32-make -jN` directly. Clean with `mingw32-make clean`. In VS Code, the default build task runs `build.bat`.
- **Required environment variables:** `VULKAN_SDK` and `GLFW_PATH` (GLFW libs are expected in `$(GLFW_PATH)/lib-mingw-w64`).
- **Configurations:** the default target is the debug build `main.exe` (`-O0 -g -D DEBUG`, objects in `build/`). `mingw32-make release` (or `build.bat release`) builds `main-release.exe` (`-O2 -D NDEBUG`, objects in `build-release/`). Each configuration has its own precompiled header in its object directory. In the debug build, asserts and Vulkan validation layers are on (`src/renderer/vk.hpp`); in the release build both are off. Keep this in mind when reading performance numbers.
- **Warnings:** project code is compiled with `-Wall -Wextra -Wpedantic -Wshadow` (`-Wmissing-field-initializers` is off, because designated initializers leave fields out on purpose). Vendored headers are included with `-isystem` and `src/imgui/` is compiled without warnings. Both configurations must build with no warnings; the release build at `-O2` reports some warnings the debug build doesn't.
- **Shaders are built by make.** `slang.spv` and `cull.spv` are rebuilt from `shaders/shader.slang` and `shaders/cull.slang` (slangc) as part of both builds; `mingw32-make shaders` or `compile_shaders.bat` builds only them. They are written to the repo root.
- At startup `main()` sets the working directory to the folder containing the exe, so `slang.spv`, `cull.spv`, `textures/`, `models/` and `imgui.ini` resolve next to it wherever the exe is launched from.
- **Tests:** `mingw32-make test` builds and runs `tests.exe`, the doctest unit tests in `tests/`. They cover `ArenaAllocator`, `floorDiv` / `floorMod`, `raycast` and `ChunkMeshGenerator`. They use the debug configuration and PCH and link only the core objects listed in `TEST_CORE_SRCS` in the Makefile. A new test file in `tests/` is picked up automatically; a test that needs another `src/` file must add it to `TEST_CORE_SRCS`. Rendering, streaming and input have no tests, so verify those by building and running the app.

## Code layout

- `src/core/`: world, chunks, terrain generation (`chunk_generator`), greedy mesher (`chunk_mesh_generator`), streaming thread (`chunk_system`), input, raycast, math helpers.
- `src/renderer/`: `Renderer` (frame loop, indirect draw), `MeshAllocator` (sub-allocates the shared GPU buffers through `core/arena_allocator`), `TextureManager`, ImGui wrapper (`gui.hpp`), `pipelines/main_pipeline`.
- `src/renderer/gpu/`: thin RAII wrappers named `Gpu*` (`GpuContext`, `GpuSwapchain`, `GpuResource`, `GpuPipeline`, …).
- `src/ecs/`: a minimal entity/component system. It is currently unused; no entities are added to the scene and the robot model is not loaded.
- `tests/`: unit tests (doctest). `world_fixture.hpp` builds a `World` from hand-made chunks without terrain generation.
- `src/app.hpp`: owns everything and contains the main loop and the block break/place logic.
- **Vendored code (don't edit or review):** `include/` (glm, imgui headers, tiny_gltf, json.hpp, perlin_noise.hpp, stb, doctest.h) and `src/imgui/`. The stb and tinygltf implementations are compiled only in `src/core/mesh.cpp`.

## Conventions

- Include paths are relative to `src/` (`-Isrc`), using angle brackets: `#include <core/world.hpp>`.
- Use `#ifndef FOO_HPP` header guards, not `#pragma once`.
- Class names are PascalCase; methods and variables are camelCase. Indentation is 2 spaces, except `renderer.cpp`, which uses tabs.
- Don't write comments in code.
- `src/pch.hpp` is force-included into every translation unit (std, glm, vulkan_raii, GLFW). Adding to it rebuilds everything.
- `GLM_FORCE_DEPTH_ZERO_TO_ONE` and `GLM_ENABLE_EXPERIMENTAL` are defined in the PCH.
- vulkan-hpp is built with `VULKAN_HPP_NO_STRUCT_CONSTRUCTORS`, so Vulkan structs must use designated initializers (`.field = ...`) in declaration order.
- Commit messages follow the format in **Workflow → Commit** below.

## Key facts

- **Units:** `VOXEL_SIZE = 10` world units per voxel and `CHUNK_SIZE = 16` voxels. Convert world positions to voxels by dividing by `VOXEL_SIZE`. Convert voxels to chunks with `floorDiv` / `floorMod` (`core/math.hpp`), never with `/` or `%`, because coordinates can be negative.
- **Voxel index** inside a chunk: `x*256 + y*16 + z`.
- **Block types and textures** live in `core/block.hpp`. `BlockType` lists the blocks (`BLOCK_AIR` = 0, `BLOCK_GRASS` = 1, `BLOCK_DIRT` = 2); use these names, not literals. `BLOCK_TEXTURE_PATHS` is the ordered texture list, which `Renderer` loads into descriptor slots 0..N-1. `BLOCK_FACE_TEXTURES` maps each block type to its top/bottom/side texture. The mesher writes that 0-based slot into `Vertex::textureId`, and the shader samples `textures[NonUniformResourceIndex(textureId)]` with no remapping. To add a block or texture, extend these tables; `MAX_TEXTURES` (`renderer/texture.hpp`) must match the `textures[4096]` array in the shader.
- **Threads:** `ChunkSystem::run` runs on its own thread with a 16 ms tick. It is the only owner of the `World`, the `MeshAllocator` and the mesh allocations. It generates terrain, meshes chunks, uploads them, and sends results to the renderer through one ordered `ThreadQueue<ChunkMeshMessage> meshQueue` (a variant of `ChunkMeshLoad` / `ChunkMeshUnload`). `Renderer::render()` drains it every frame. Other threads talk to the chunk thread only through `pushEdit()`, `setViewerPosition()` and the queues. Block edits in `App::gameplay()` are sent as `BlockEdit` requests through `pushEdit()`, and the chunk thread runs the raycast and remesh itself. Never touch the `World` or the `MeshAllocator` from the main thread.
- **GPU memory:** the renderer owns one 64 MiB vertex buffer and one 32 MiB index buffer. All chunks are sub-allocated from them and drawn with a single `drawIndexedIndirect`, after CPU frustum culling.
- **Terrain:** only chunks that contain the heightmap surface are generated. Solid chunks below the surface do not exist (#13).
- `imgui.ini` is rewritten on every run. Don't commit changes to it.

## Known issues

A full review is in `REVIEW.md`, and its findings are tracked as GitHub issues #2–#28 on `Arillaxe/vixy`, which is a public repo. Labels are `priority: *`, `area: *` and `performance`. Before fixing or refactoring something, check `gh issue list`; the work then follows the **Workflow** below. New issues have been filed since, so the numbers go past #28. The critical issues (#2–#7) concern threading and GPU memory lifetime. Most are fixed, but be careful when touching `chunk_system`, `mesh_allocator` or the renderer's queue handling.

## Workflow

Each task is a GitHub issue. Handle each one with these steps, in order.

1. **Get the issue.** The user assigns an issue number. Read it with `gh issue view <N> --comments`. Then start from an up-to-date `main` with a clean working tree: `git checkout main && git pull`. If there are uncommitted changes, ask before touching them.
2. **Plan.** Read the code the issue points at. Its permalinks point to commit `f91528e`, so line numbers may have drifted. Work out the approach and the files that will change. If the issue is ambiguous, has more than one reasonable design, or would grow well beyond its stated scope, ask the user before writing code. Fix only what the issue describes, and mention any unrelated problems you notice instead of fixing them.
3. **Implement.** Create a branch from `main` named `<type>/<N>-<short-slug>`, for example `fix/5-empty-mesh-assert` or `perf/11-column-cache`. Make the change, then:
   - build with `build.bat`. The build must succeed with no new warnings;
   - shaders are rebuilt by make; check the build output for slangc errors if `shaders/` changed;
   - run `mingw32-make test`; all tests must pass. Add or update tests when the change touches the tested core logic;
   - verify what you can. Rendering results can't be seen, so tell the user exactly what to try in the running app.
4. **Review.** Before committing, spawn a fresh review subagent (`Agent` tool, general-purpose). Give it the issue number and tell it to review the uncommitted diff (`git diff`) for correctness, thread safety, Vulkan lifetime and synchronization problems, and anything the issue required but the change missed. Fix the real findings. If the fixes are substantial, run the review again. Don't commit until the review is clean or the remaining findings have been explained as not applicable.
5. **Commit.** One commit per issue unless the user says otherwise. The message is **one line**: a conventional prefix (`fix:`, `feat:`, `perf:`, `refactor:`, `chore:`, `docs:`, `build:`, `test:`), then a short, simple description in lowercase and imperative mood, with no trailing period. Example: `fix: skip gpu allocation for empty chunk meshes`. **No `Co-Authored-By` or any other trailer, and no body.** Stage only files that belong to the change: never `imgui.ini`, `build/`, `build-release/`, `main.exe`, `main-release.exe`, `tests.exe`, `*.spv` or `*.gch`.
6. **Open a PR.** Push with `git push -u origin <branch>`. Then run `gh pr create --base main`, using the commit message as the title. The body has a short summary of the change, how it was verified (plus what the user should test by hand), and `Fixes #<N>` so that merging closes the issue. **No "Generated with Claude Code" footer or other AI attribution.** Report the PR link and **stop**. The user reviews and merges. Never merge PRs yourself and never push to `main`.

If `gh` is not found, call it by its full path: `"/c/Program Files/GitHub CLI/gh.exe"`.

## Controls

WASD to move, Space / Left Ctrl for up / down, mouse to look, left click to break a block, right click to place one, E to toggle the cursor, Esc to quit.
