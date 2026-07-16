# PmxMod

[English](./README.md) | **[한국어](./README.ko.md)** | [日本語](./README.ja.md) | [中文](./README.zh.md)

PmxMod는 C++23으로 작성한 PMX/VMD 모델 뷰어입니다. 현재 OpenGL, DirectX 11, DirectX 12, Vulkan 렌더러를 지원합니다.

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

프로젝트 루트의 `vcpkg.json` manifest를 사용합니다. 라이브러리를 하나씩 직접 설치할 필요는 없고, CMake configure 때 vcpkg toolchain 파일을 지정하면 선택한 triplet에 맞춰 manifest 의존성이 설치됩니다.

먼저 Vulkan SDK를 설치합니다.

```powershell
winget install --id KhronosGroup.VulkanSDK -e
```

Vulkan SDK 설치 후에는 새 PowerShell 창을 열어야 CMake가 Vulkan SDK 환경 변수를 인식할 수 있습니다.

## 빌드

vcpkg가 `C:/vcpkg`에 설치되어 있다면:

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

vcpkg가 다른 위치에 있다면 toolchain 경로만 바꿉니다.

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

VS Code CMake Tools를 사용하는 경우:

1. `CMake: Delete Cache and Reconfigure`를 실행합니다.
2. Release 구성을 선택합니다.
3. `PmxMod` 타깃을 빌드합니다.

vcpkg 경로가 기본값과 다르면 VS Code 설정에 toolchain 파일을 추가합니다.

```json
{
  "cmake.configureSettings": {
    "CMAKE_TOOLCHAIN_FILE": "D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
  }
}
```

## 셰이더 패키지

실행 중 셰이더 패키지는 `resource/shaders/`에서 로드됩니다. 패키지는 `package.json` manifest와 하나 이상의 HLSL 효과로 구성됩니다. 내장 모델, 엣지, 지면 그림자 셰이더는 패키지가 아닌 단독 엔진 자원으로 `resource/internal/shaders`에 있습니다.

후처리 예제나 틀은 `resource/shaders/` 아래의 별도 패키지로 추가할 수 있습니다. 실행 리소스는 `SyncResources` 타깃이 `resource/`에서 CMake 빌드 디렉터리로 복사합니다.

## 참고

- GLFW, GLAD, GLM, Bullet, miniaudio, nlohmann-json, SPIRV-Cross, stb 의존성은 `vcpkg.json`에 선언되어 있습니다.
- Vulkan과 DXC는 설치된 Vulkan SDK를 통해 찾습니다.
- OpenGL을 화면 기준 렌더러로 사용합니다. DirectX 11, DirectX 12, Vulkan은 API가 허용하는 범위에서 모델, 엣지, 지면 그림자, 텍스처, 깊이, 스텐실, 블렌드, MSAA 동작을 OpenGL 기준에 맞춥니다.
- DirectX 12와 Vulkan은 별도 MSAA 렌더 타깃에 그린 뒤 스왑체인 이미지로 resolve합니다. OpenGL의 기본 framebuffer 흐름과 구현 방식은 다르지만, sample count 정책과 최종 화면 결과를 맞추는 방향입니다.
