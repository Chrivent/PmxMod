# PmxMod

**[English](./README.md)** | [한국어](./README.ko.md)

PMX/VMD model viewer built with C++23. The project currently renders through OpenGL, DirectX 11, DirectX 12, and Vulkan.

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

Install the required packages with vcpkg:

```powershell
C:\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio nlohmann-json --triplet x64-windows
winget install --id KhronosGroup.VulkanSDK -e
```

If vcpkg is installed somewhere else, use that path instead:

```powershell
D:\dev\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio nlohmann-json --triplet x64-windows
winget install --id KhronosGroup.VulkanSDK -e
```

After installing the Vulkan SDK, open a new PowerShell window so CMake can see the SDK environment variables.

## Build

By default, CMake expects vcpkg at `C:/vcpkg`.

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target PmxMod
```

If vcpkg is installed in another directory, pass `VCPKG_ROOT` when configuring:

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DVCPKG_ROOT=D:/dev/vcpkg
cmake --build cmake-build-release --target PmxMod
```

In VS Code with CMake Tools:

1. Run `CMake: Delete Cache and Reconfigure`.
2. Select the Release configuration.
3. Build target `PmxMod`.

For a custom vcpkg location, add this to VS Code settings:

```json
{
  "cmake.configureSettings": {
    "VCPKG_ROOT": "D:/dev/vcpkg"
  }
}
```

## Notes

- `stb` is found through vcpkg's `FindStb.cmake`.
- `miniaudio` is header-only and is found by locating `miniaudio.h`.
- `nlohmann-json` parses the runtime GUI language files.
- Vulkan is found through the installed Vulkan SDK.
- OpenGL is used as the visual reference renderer. DirectX 11, DirectX 12, and Vulkan keep matching model, edge, ground shadow, texture, depth, stencil, blend, and MSAA behavior where the APIs allow it.
- DirectX 12 and Vulkan use explicit MSAA render targets and resolve into the swapchain image. This differs from OpenGL's default framebuffer flow, but follows the same sample-count policy and visual result.
- Runtime resources are copied from `resource/` into the CMake build directory by the `SyncResources` target.
