# PmxMod

**[English](./README.md)** | [한국어](./README.ko.md) | [日本語](./README.ja.md) | [中文](./README.zh.md)

PmxMod is a C++23 PMX/VMD model viewer. It currently supports OpenGL, DirectX 11, DirectX 12, and Vulkan renderers.

## Requirements

- Windows
- Visual Studio C++ toolchain
- CMake 4.1.2 or newer
- vcpkg
- Vulkan SDK

## Renderer Support

- OpenGL: supported
- DirectX 11: supported
- DirectX 12: supported
- Vulkan: supported

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

In VS Code with CMake Tools:

1. Run `CMake: Delete Cache and Reconfigure`.
2. Select the Release configuration.
3. Build target `PmxMod`.

For a custom vcpkg location, add the toolchain file to VS Code settings:

```json
{
  "cmake.configureSettings": {
    "CMAKE_TOOLCHAIN_FILE": "D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
  }
}
```

## Shader Packages

Runtime shader packages are loaded from `resource/shaders/`. A package contains a `package.json` manifest and one or more HLSL effects. The built-in model, edge, and ground-shadow shaders are standalone engine resources under `resource/internal/shaders` and are not packages.

Post-process examples and stubs can be added as separate packages under `resource/shaders/`. Runtime resources are copied from `resource/` into the CMake build directory by the `SyncResources` target.

## Notes

- Dependencies such as GLFW, GLAD, GLM, Bullet, miniaudio, nlohmann-json, SPIRV-Cross, and stb are declared in `vcpkg.json`.
- Vulkan and DXC are found through the installed Vulkan SDK.
- OpenGL is used as the visual reference renderer. DirectX 11, DirectX 12, and Vulkan keep matching model, edge, ground shadow, texture, depth, stencil, blend, and MSAA behavior where the APIs allow it.
- DirectX 12 and Vulkan use explicit MSAA render targets and resolve into the swapchain image. This differs from OpenGL's default framebuffer flow, but follows the same sample-count policy and visual result.
