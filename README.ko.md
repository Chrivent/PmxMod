# PmxMod

[English](./README.md) | **[한국어](./README.ko.md)** | [日本語](./README.ja.md) | [中文](./README.zh.md)

PmxMod는 Windows용 C++23 PMX/VMD 뷰어 및 모션 재생 프로그램입니다. OpenGL, Direct3D 11, Direct3D 12, Vulkan에서 공통 HLSL 셰이더 패키지 계약을 사용합니다.

## 주요 기능

- PMX 모델과 VMD 모델/카메라 모션 불러오기
- 네 가지 렌더링 API 실행 중 전환
- 내장 모델, 엣지, 지면 그림자 렌더링
- depth, velocity, 중간 및 history 리소스를 사용하는 순차 후처리 체인
- 피사계 심도, 모션 블러, 그레이스케일 예제를 포함한 설치형 HLSL 셰이더 패키지
- `.pmscene` 씬 불러오기 및 저장

## 요구 사항

- Windows
- Visual Studio C++ toolchain
- CMake 4.1.2 이상
- vcpkg
- Vulkan SDK
- 아래 렌더러 중 하나 이상을 지원하는 GPU와 드라이버

## 렌더러 지원 상태

| 렌더러 | 요구 버전 | 셰이더 경로 |
| --- | --- | --- |
| OpenGL | 4.6 Core Profile | HLSL Shader Model 6.0 → SPIR-V → GLSL 4.60 |
| Direct3D 11 | Feature Level 11.0, 지원 시 11.1 | HLSL Shader Model 5.0 |
| Direct3D 12 | 최소 Feature Level 11.0, 최대 12.2까지 감지 | Shader Model 6.0, 5.1 대체 경로 |
| Vulkan | 1.3 | HLSL Shader Model 6.0 → SPIR-V |

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

JetBrains Rider를 사용하는 경우:

1. `설정 | 빌드, 실행, 배포 | CMake`를 엽니다.
2. 사용할 프로필의 CMake 옵션에 vcpkg toolchain 경로를 추가합니다.
3. CMake 프로젝트를 다시 불러오고 `PmxMod` 타깃을 빌드합니다.

예시 CMake 옵션:

```text
-DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## 셰이더 패키지

실행 중 셰이더 패키지는 `resource/shaders/`에서 로드됩니다. 패키지는 `package.json` manifest와 하나 이상의 HLSL 효과로 구성됩니다. 내장 모델, 엣지, 지면 그림자 셰이더는 패키지가 아닌 단독 엔진 자원으로 `resource/internal/shaders`에 있습니다.

후처리 예제나 틀은 `resource/shaders/` 아래의 별도 패키지로 추가할 수 있습니다. 실행 리소스는 `SyncResources` 타깃이 `resource/`에서 CMake 빌드 디렉터리로 복사합니다.

## 테스트

Viewer 계약 테스트는 GoogleTest를 사용하며 일반 프로그램 빌드에서는 비활성화됩니다. 다음 명령으로 활성화하고 실행할 수 있습니다.

```powershell
cmake -S . -B cmake-build-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DPMXMOD_BUILD_TESTS=ON
cmake --build cmake-build-test --target PmxModViewerTests
ctest --test-dir cmake-build-test --output-on-failure
```

첫 테스트 묶음은 GPU 디바이스를 생성하지 않고 API 독립 후처리 계획, 파라미터 검증, 리소스 경로, 해상도 규칙, temporal history 상태를 검사합니다.

## 참고

- GLFW, GLAD, GLM, Bullet, GoogleTest, miniaudio, nlohmann-json, SPIRV-Cross, stb 의존성은 `vcpkg.json`에 선언되어 있습니다.
- Vulkan과 DXC는 설치된 Vulkan SDK를 통해 찾습니다.
- OpenGL을 화면 기준 렌더러로 사용합니다. DirectX 11, DirectX 12, Vulkan은 API가 허용하는 범위에서 모델, 엣지, 지면 그림자, 텍스처, 깊이, 스텐실, 블렌드, MSAA 동작을 OpenGL 기준에 맞춥니다.
- DirectX 12와 Vulkan은 별도 MSAA 렌더 타깃에 그린 뒤 스왑체인 이미지로 resolve합니다. OpenGL의 기본 framebuffer 흐름과 구현 방식은 다르지만, sample count 정책과 최종 화면 결과를 맞추는 방향입니다.
