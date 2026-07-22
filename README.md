# PmxMod

**[English](./README.md)** | [한국어](./README.ko.md) | [日本語](./README.ja.md) | [中文](./README.zh.md)

PmxMod is a Windows C++23 PMX/VMD viewer and motion playback application. It provides a common HLSL shader-package contract across OpenGL, Direct3D 11, Direct3D 12, and Vulkan.

## Features

- PMX model and VMD model/camera motion loading
- Runtime switching between four rendering APIs
- Built-in model, edge, and ground-shadow rendering
- Ordered post-process chains with depth, velocity, intermediate, and history resources
- Installable HLSL shader packages, including depth of field, motion blur, and grayscale examples
- `.pmscene` scene loading and saving

## Requirements

- Windows
- Visual Studio C++ toolchain
- CMake 4.1.2 or newer
- vcpkg
- Vulkan SDK
- A GPU and driver supporting at least one renderer listed below

## Renderer Support

| Renderer | Required version | Shader path |
| --- | --- | --- |
| OpenGL | 4.6 Core Profile | HLSL Shader Model 6.0 → SPIR-V → GLSL 4.60 |
| Direct3D 11 | Feature Level 11.0; 11.1 when available | HLSL Shader Model 5.0 |
| Direct3D 12 | Feature Level 11.0 minimum; detects up to 12.2 | Shader Model 6.0 with a 5.1 fallback |
| Vulkan | 1.3 | HLSL Shader Model 6.0 → SPIR-V |

## Dependencies

The project uses the `vcpkg.json` manifest in the repository root. You do not need to install each library one by one; configure CMake with the vcpkg toolchain file and vcpkg will install the manifest dependencies for the selected triplet.

Install the Vulkan SDK first:

```powershell
winget install --id KhronosGroup.VulkanSDK -e
```

After installing the Vulkan SDK, open a new PowerShell window so CMake can see the SDK environment variables.

## Build

If vcpkg is installed at `C:/vcpkg`:

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

If vcpkg is installed elsewhere, replace the toolchain path:

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

In JetBrains Rider:

1. Open `Settings | Build, Execution, Deployment | CMake`.
2. Add the vcpkg toolchain path to the selected profile's CMake options.
3. Reload the CMake project and build target `PmxMod`.

Example CMake option:

```text
-DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## Shader Packages

Runtime shader packages are loaded from `resource/shaders/`. A package contains a `package.json` manifest and one or more HLSL effects. The built-in model, edge, and ground-shadow shaders are standalone engine resources under `resource/internal/shaders` and are not packages.

Post-process examples and stubs can be added as separate packages under `resource/shaders/`. Runtime resources are copied from `resource/` into the CMake build directory by the `SyncResources` target.

## Tests

Viewer contract tests use GoogleTest and are disabled for normal application builds. Enable and run them with:

```powershell
cmake -S . -B cmake-build-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DPMXMOD_BUILD_TESTS=ON
cmake --build cmake-build-test
ctest --test-dir cmake-build-test --output-on-failure
```

The initial suite validates API-independent post-process planning, parameter validation, resource routing, resolution rules, and temporal history state without creating a GPU device.

## Notes

- Dependencies such as GLFW, GLAD, GLM, Bullet, GoogleTest, miniaudio, nlohmann-json, SPIRV-Cross, and stb are declared in `vcpkg.json`.
- Vulkan and DXC are found through the installed Vulkan SDK.
- OpenGL is used as the visual reference renderer. DirectX 11, DirectX 12, and Vulkan keep matching model, edge, ground shadow, texture, depth, stencil, blend, and MSAA behavior where the APIs allow it.
- DirectX 12 and Vulkan use explicit MSAA render targets and resolve into the swapchain image. This differs from OpenGL's default framebuffer flow, but follows the same sample-count policy and visual result.
