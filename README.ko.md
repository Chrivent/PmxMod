# PmxMod

[English](./README.md) | **[한국어](./README.ko.md)** | [日本語](./README.ja.md) | [中文](./README.zh.md)

[![Windows CI](https://github.com/Chrivent/PmxMod/actions/workflows/windows-ci.yml/badge.svg?branch=main)](https://github.com/Chrivent/PmxMod/actions/workflows/windows-ci.yml)

PmxMod는 PMX 모델을 불러오고 VMD 모델·카메라 모션을 재생하며 HLSL 후처리 효과를 적용하는 Windows용 C++23 프로그램입니다. 같은 씬과 셰이더 패키지 계약을 OpenGL, Direct3D 11, Direct3D 12, Vulkan에서 사용합니다.

## 실행 영상

썸네일이나 제목을 클릭하면 YouTube에서 영상을 볼 수 있습니다.

<table>
  <tr>
    <td align="center" width="50%">
      <a href="https://www.youtube.com/watch?v=td8ko49hFW4"><img src="https://i.ytimg.com/vi/td8ko49hFW4/hqdefault.jpg" alt="실행 영상 1" width="480"></a><br>
      <a href="https://www.youtube.com/watch?v=td8ko49hFW4">실행 영상 1</a>
    </td>
    <td align="center" width="50%">
      <a href="https://www.youtube.com/watch?v=GZMTVKHpLDU"><img src="https://i.ytimg.com/vi/GZMTVKHpLDU/hqdefault.jpg" alt="실행 영상 2" width="480"></a><br>
      <a href="https://www.youtube.com/watch?v=GZMTVKHpLDU">실행 영상 2</a>
    </td>
  </tr>
  <tr>
    <td align="center" width="50%">
      <a href="https://www.youtube.com/watch?v=lUkcElzVQyA"><img src="https://i.ytimg.com/vi/lUkcElzVQyA/hqdefault.jpg" alt="실행 영상 3" width="480"></a><br>
      <a href="https://www.youtube.com/watch?v=lUkcElzVQyA">실행 영상 3</a>
    </td>
    <td align="center" width="50%">
      <a href="https://www.youtube.com/watch?v=C6waA9sEBlQ"><img src="https://i.ytimg.com/vi/C6waA9sEBlQ/hqdefault.jpg" alt="실행 영상 4" width="480"></a><br>
      <a href="https://www.youtube.com/watch?v=C6waA9sEBlQ">실행 영상 4</a>
    </td>
  </tr>
</table>

## 주요 기능

- PMX 모델 불러오기와 VMD 모델·카메라 모션 재생
- OpenGL, Direct3D 11, Direct3D 12, Vulkan 실행 중 전환
- 내장 모델, 엣지, 지면 그림자 켜기·끄기
- depth, velocity, 중간 리소스와 temporal history를 사용하는 순차 후처리
- 폴더 단위 HLSL 셰이더 패키지
- `.pmscene` 씬 불러오기와 저장
- 타임라인, 보간 곡선, 오디오 파형, 정보, 재생 패널

## 기본 사용법

1. `PmxMod.exe`와 `resource` 폴더를 같은 위치에 둔 상태로 프로그램을 실행합니다.
2. **파일 > 새로 만들기** 또는 **파일 > 열기**로 씬을 준비합니다.
3. 모델 패널에서 PMX 모델을 추가하고 각 모델 행에서 VMD 모션을 지정합니다.
4. 카메라 패널에서 카메라 VMD를 추가하고 사용할 후처리 효과를 체크합니다.
5. 렌더러 메뉴에서 API를 선택하고 재생 패널에서 재생을 제어합니다.

첫 실행 언어는 Windows 표시 언어를 따르며 **보기 > 언어**에서 바꿀 수 있습니다.

## 지원 파일

| 파일 | 용도 |
| --- | --- |
| `.pmx` | MMD 모델 |
| `.vmd` | 모델 또는 카메라 모션 |
| `.pmscene` | 모델·모션·카메라·음악 경로와 모델 배율을 저장하는 PmxMod 씬 |

`.pmscene`은 외부 파일의 경로만 저장하며 파일 자체를 포함하지 않습니다. 저장된 경로에서 원본 파일을 읽을 수 있어야 합니다.

## 셰이더 패키지 설치

셰이더 패키지 폴더 전체를 다음 위치에 넣고 PmxMod를 다시 실행합니다.

```text
PmxMod.exe
resource/
└─ shaders/
   └─ 패키지-폴더/
      └─ package.json
```

유효한 효과는 카메라 패널에 각각 한 행으로 표시됩니다. 여러 효과를 함께 체크할 수 있으며 목록 순서대로 적용됩니다. 기본 제공 패키지에는 피사계 심도, 깊이 시각화, 모션 블러가 들어 있습니다.

`resource/internal/shaders`의 모델, 엣지, 지면 그림자 셰이더는 설치형 패키지가 아닌 엔진 리소스입니다. 패키지 작성 방법은 [셰이더 패키지 계약](./resource/shaders/README.ko.md)을 참고하세요.

## 렌더러 요구 사항

| 렌더러 | 최소 요구 사항 | 셰이더 경로 |
| --- | --- | --- |
| OpenGL | 4.6 Core Profile | HLSL Shader Model 6.0 → SPIR-V → GLSL 4.60 |
| Direct3D 11 | Feature Level 11.0 | HLSL Shader Model 5.0 |
| Direct3D 12 | Feature Level 11.0 | 지원 시 Shader Model 6.0, 그 외 5.1 |
| Vulkan | 1.3 | HLSL Shader Model 6.0 → SPIR-V |

## 소스 빌드

필요한 항목:

- Windows와 Visual Studio C++ toolchain
- CMake 4.1.2 이상
- vcpkg
- Vulkan SDK

Vulkan SDK를 설치한 뒤 MSVC와 SDK 환경을 함께 사용할 수 있도록 새 **Developer PowerShell for Visual Studio**를 엽니다.

```powershell
winget install --id KhronosGroup.VulkanSDK -e
```

루트의 `vcpkg.json`에 선언된 의존성은 CMake configure 과정에서 설치됩니다. vcpkg toolchain을 지정해 configure하고 빌드하며, 설치 위치가 다르면 `C:/vcpkg`를 바꿉니다.

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

빌드할 때 `resource`, GLFW 런타임과 `dxcompiler.dll`이 실행 파일 옆으로 복사됩니다. JetBrains Rider에서는 같은 `CMAKE_TOOLCHAIN_FILE` 옵션을 활성 CMake 프로필에 넣고 `PmxMod` 타깃을 선택합니다.

## 명령행 실행

```text
PmxMod [--scene <file.pmscene>] [--renderer <opengl|dx11|dx12|vulkan>]
       [--benchmark <frames>] [--warmup <frames>]
```

`PmxMod --help`로 형식을 확인할 수 있습니다. 벤치마크 옵션은 지정한 프레임 수만큼 고정 실행한 성능 결과를 콘솔에 출력합니다.

## 테스트

GoogleTest는 Core 파싱과 런타임 동작, Program 계약, API 독립 렌더링 계획, API별 descriptor, 네 백엔드용 HLSL 컴파일을 검사합니다.

```powershell
cmake -S . -B cmake-build-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DPMXMOD_BUILD_TESTS=ON
cmake --build cmake-build-test
ctest --test-dir cmake-build-test --output-on-failure
```
