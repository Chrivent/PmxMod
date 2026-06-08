# PmxMod

[English](./README.md) | **[한국어](./README.ko.md)**

C++23으로 작성한 PMX/VMD 모델 뷰어입니다. 현재 OpenGL, DirectX 11, DirectX 12, Vulkan 렌더러를 지원합니다.

## 요구 사항

- Windows
- Visual Studio C++ toolchain
- CMake 4.1.2 이상
- vcpkg
- Vulkan SDK

## 렌더러 지원 상태

- OpenGL: 지원
- DirectX 11: 지원
- DirectX 12: 지원
- Vulkan: 지원

## 의존성 설치

vcpkg가 `C:/vcpkg`에 설치되어 있다면 아래 명령으로 필요한 패키지를 설치합니다.

```powershell
C:\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio --triplet x64-windows
winget install --id KhronosGroup.VulkanSDK -e
```

vcpkg가 다른 위치에 설치되어 있다면 해당 경로의 `vcpkg.exe`를 사용하면 됩니다.

```powershell
D:\dev\vcpkg\vcpkg.exe install glfw3 glad glm bullet3 stb miniaudio --triplet x64-windows
winget install --id KhronosGroup.VulkanSDK -e
```

Vulkan SDK 설치 후에는 새 PowerShell 창을 열어야 CMake가 Vulkan SDK 환경 변수를 인식할 수 있습니다.

## 빌드

기본적으로 CMake는 vcpkg가 `C:/vcpkg`에 있다고 가정합니다.

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target PmxMod
```

vcpkg가 다른 위치에 있다면 configure 단계에서 `VCPKG_ROOT`를 지정합니다.

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

## 참고

- `stb`는 vcpkg의 `FindStb.cmake`를 통해 찾습니다.
- `miniaudio`는 header-only 라이브러리라 `miniaudio.h` 위치를 찾아 include 경로로 추가합니다.
- Vulkan은 설치된 Vulkan SDK를 통해 찾습니다.
- OpenGL을 화면 기준 렌더러로 사용합니다. DirectX 11, DirectX 12, Vulkan은 API가 허용하는 범위에서 모델, 엣지, 지면 그림자, 텍스처, 깊이, 스텐실, 블렌드, MSAA 동작을 OpenGL 기준에 맞춥니다.
- DirectX 12와 Vulkan은 별도 MSAA 렌더 타깃에 그린 뒤 스왑체인 이미지로 resolve합니다. OpenGL의 기본 framebuffer 흐름과 구현 방식은 다르지만, sample count 정책과 최종 화면 결과를 맞추는 방향입니다.
- 실행 리소스는 `SyncResources` 타깃이 `resource/`에서 CMake 빌드 디렉터리로 복사합니다.
