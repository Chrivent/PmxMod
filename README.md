# PmxMod

PMX/VMD 모델을 로드해 OpenGL 또는 DirectX 11 렌더러로 재생하는 C++20 프로젝트입니다.

## Requirements

- Windows
- Visual Studio C++ toolchain
- CMake 3.20 이상
- vcpkg installed at `C:/vcpkg`

## Dependencies

Install the required packages with vcpkg:

```powershell
C:\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio --triplet x64-windows
```

The project expects vcpkg packages under:

```text
C:/vcpkg/installed/x64-windows
```

## Build

Configure and build with CMake:

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target PmxMod
```

In VS Code with CMake Tools:

1. Run `CMake: Delete Cache and Reconfigure`.
2. Select the Release configuration.
3. Build target `PmxMod`.

## Notes

- `stb` is found through vcpkg's `FindStb.cmake`.
- `miniaudio` is header-only and is found by locating `miniaudio.h`.
- Runtime resources are copied from `resource/` into the CMake build directory by the `SyncResources` target.
