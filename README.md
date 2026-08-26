# PmxMod

**[English](./README.md)** | [한국어](./README.ko.md) | [日本語](./README.ja.md) | [中文](./README.zh.md)

[![Windows CI](https://github.com/Chrivent/PmxMod/actions/workflows/windows-ci.yml/badge.svg?branch=main)](https://github.com/Chrivent/PmxMod/actions/workflows/windows-ci.yml)

PmxMod is a Windows C++23 application for loading PMX models, playing VMD model and camera motions, and applying HLSL post-process effects. The same scene and shader package contract runs on OpenGL, Direct3D 11, Direct3D 12, and Vulkan.

## Features

- PMX model loading and VMD model/camera motion playback
- Runtime switching between OpenGL, Direct3D 11, Direct3D 12, and Vulkan
- Built-in model, edge, and ground-shadow toggles
- Ordered post-process effects with depth, velocity, intermediate, and temporal history resources
- Folder-based HLSL shader packages
- Scene loading and saving with `.pmscene` files
- Timeline, interpolation-curve, audio-waveform, information, and playback panels

## Basic usage

1. Run `PmxMod.exe` with its `resource` directory beside it.
2. Use **File > New** or **File > Open** to start a scene.
3. Add PMX models from the Model panel, then assign VMD motions from each model row.
4. Add a camera VMD and enable post-process effects from the Camera panel.
5. Select the rendering API from the Renderer menu and control playback from the Playback panel.

The interface follows the Windows display language on first launch. It can be changed from **View > Language**.

## Supported files

| File | Purpose |
| --- | --- |
| `.pmx` | MMD model |
| `.vmd` | Model or camera motion |
| `.pmscene` | PmxMod scene containing model, motion, camera, music paths, and model scales |

A `.pmscene` file references external assets; it does not embed them. Keep the referenced files available at their saved paths.

## Shader packages

To install a shader package, place the entire package directory here and restart PmxMod:

```text
PmxMod.exe
resource/
└─ shaders/
   └─ package-directory/
      └─ package.json
```

Each valid effect in the package appears as a separate row in the Camera panel. Effects can be enabled together and are applied in list order. The included packages provide depth of field, depth visualization, motion blur, and grayscale.

The model, edge, and ground-shadow shaders under `resource/internal/shaders` are engine resources, not installable packages. See the [shader package contract](./resource/shaders/README.ko.md) for the schema and HLSL bindings.

## Renderer requirements

| Renderer | Minimum requirement | Shader path |
| --- | --- | --- |
| OpenGL | 4.6 Core Profile | HLSL Shader Model 6.0 → SPIR-V → GLSL 4.60 |
| Direct3D 11 | Feature Level 11.0 | HLSL Shader Model 5.0 |
| Direct3D 12 | Feature Level 11.0 | Shader Model 6.0 when supported, otherwise 5.1 |
| Vulkan | 1.3 | HLSL Shader Model 6.0 → SPIR-V |

## Build from source

Requirements:

- Windows and the Visual Studio C++ toolchain
- CMake 4.1.2 or newer
- vcpkg
- Vulkan SDK

Install the Vulkan SDK, then open a new **Developer PowerShell for Visual Studio** so the MSVC and SDK environments are available:

```powershell
winget install --id KhronosGroup.VulkanSDK -e
```

The repository's `vcpkg.json` installs the declared dependencies during CMake configuration. Configure and build with the vcpkg toolchain, replacing `C:/vcpkg` if needed:

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

The build copies `resource`, the GLFW runtime, and `dxcompiler.dll` beside the executable. In JetBrains Rider, add the same `CMAKE_TOOLCHAIN_FILE` option to the active CMake profile and select the `PmxMod` target.

## Command line

```text
PmxMod [--scene <file.pmscene>] [--renderer <opengl|dx11|dx12|vulkan>]
       [--benchmark <frames>] [--warmup <frames>]
```

Use `PmxMod --help` to print the syntax. Benchmark options run a fixed-frame performance pass and print the result to the console.

## Tests

The GoogleTest suite covers Core parsing and runtime behavior, Program contracts, API-independent rendering plans, API descriptors, and HLSL compilation for all four backends.

```powershell
cmake -S . -B cmake-build-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DPMXMOD_BUILD_TESTS=ON
cmake --build cmake-build-test
ctest --test-dir cmake-build-test --output-on-failure
```
