# PmxMod

PMX/VMD model viewer built with C++20. The project can render through OpenGL or DirectX 11.

## Requirements

- Windows
- Visual Studio C++ toolchain
- CMake 3.20 or newer
- vcpkg

## Dependencies

Install the required packages with vcpkg:

```powershell
C:\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio --triplet x64-windows
```

If vcpkg is installed somewhere else, use that path instead:

```powershell
D:\dev\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio --triplet x64-windows
```

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
- Runtime resources are copied from `resource/` into the CMake build directory by the `SyncResources` target.

---

## 한국어

PMX/VMD 모델을 C++20로 로드하고 OpenGL 또는 DirectX 11 렌더러로 실행하는 프로젝트입니다.

### 요구 사항

- Windows
- Visual Studio C++ toolchain
- CMake 3.20 이상
- vcpkg

### 의존성 설치

vcpkg가 `C:/vcpkg`에 설치되어 있다면 아래 명령으로 필요한 패키지를 설치합니다.

```powershell
C:\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio --triplet x64-windows
```

vcpkg가 다른 위치에 설치되어 있다면 해당 경로의 `vcpkg.exe`를 사용하면 됩니다.

```powershell
D:\dev\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio --triplet x64-windows
```

### 빌드

기본적으로 CMake는 vcpkg가 `C:/vcpkg`에 있다고 가정합니다.

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target PmxMod
```

vcpkg가 다른 위치에 있다면 configure할 때 `VCPKG_ROOT`를 지정합니다.

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DVCPKG_ROOT=D:/dev/vcpkg
cmake --build cmake-build-release --target PmxMod
```

VS Code CMake Tools를 사용하는 경우:

1. `CMake: Delete Cache and Reconfigure`를 실행합니다.
2. Release 구성을 선택합니다.
3. `PmxMod` 타깃을 빌드합니다.

vcpkg 경로가 기본값과 다르면 VS Code 설정에 아래 값을 추가합니다.

```json
{
  "cmake.configureSettings": {
    "VCPKG_ROOT": "D:/dev/vcpkg"
  }
}
```

### 참고

- `stb`는 vcpkg의 `FindStb.cmake`를 통해 찾습니다.
- `miniaudio`는 header-only 라이브러리라 `miniaudio.h` 위치를 찾아 include 경로로 추가합니다.
- 실행 리소스는 `SyncResources` 타깃이 `resource/`에서 CMake 빌드 디렉터리로 복사합니다.
