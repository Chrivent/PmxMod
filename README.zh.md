# PmxMod

[English](./README.md) | [한국어](./README.ko.md) | [日本語](./README.ja.md) | **[中文](./README.zh.md)**

[![Windows CI](https://github.com/Chrivent/PmxMod/actions/workflows/windows-ci.yml/badge.svg?branch=main)](https://github.com/Chrivent/PmxMod/actions/workflows/windows-ci.yml)

PmxMod 是一款面向 Windows、使用 C++23 编写的应用程序，可加载 PMX 模型、播放 VMD 模型／相机动作并应用 HLSL 后处理效果。OpenGL、Direct3D 11、Direct3D 12 和 Vulkan 使用同一套场景与着色器包契约。

## 主要功能

- 加载 PMX 模型并播放 VMD 模型／相机动作
- 运行时切换 OpenGL、Direct3D 11、Direct3D 12 和 Vulkan
- 开关内置 model、edge 和 ground shadow
- 使用 depth、velocity、中间资源与 temporal history 的有序后处理
- 基于文件夹的 HLSL 着色器包
- 加载和保存 `.pmscene` 场景
- 时间轴、插值曲线、音频波形、信息和播放面板

## 基本用法

1. 将 `PmxMod.exe` 与 `resource` 目录放在同一位置并运行程序。
2. 使用 **File > New** 或 **File > Open** 准备场景。
3. 在 Model 面板中添加 PMX 模型，并从各模型行指定 VMD 动作。
4. 在 Camera 面板中添加相机 VMD，并勾选需要的后处理效果。
5. 从 Renderer 菜单选择 API，并在 Playback 面板控制播放。

首次启动时，界面语言跟随 Windows 显示语言，可从 **View > Language** 更改。

## 支持的文件

| 文件 | 用途 |
| --- | --- |
| `.pmx` | MMD 模型 |
| `.vmd` | 模型或相机动作 |
| `.pmscene` | 保存模型、动作、相机、音乐路径及模型缩放的 PmxMod 场景 |

`.pmscene` 只保存外部资源路径，不会嵌入文件本身。请确保原始文件仍可从保存的路径读取。

## 着色器包

将完整的着色器包目录放到以下位置，然后重新启动 PmxMod：

```text
PmxMod.exe
resource/
└─ shaders/
   └─ package-directory/
      └─ package.json
```

包中的每个有效 effect 都会在 Camera 面板中显示为独立的一行。可以同时启用多个 effect，并按列表顺序应用。随附包包含景深、depth 可视化、运动模糊和灰度效果。

`resource/internal/shaders` 中的 model、edge 和 ground-shadow 着色器属于引擎资源，不是可安装包。schema 与 HLSL binding 请参阅[着色器包契约（韩文）](./resource/shaders/README.ko.md)。

## 渲染器要求

| 渲染器 | 最低要求 | 着色器路径 |
| --- | --- | --- |
| OpenGL | 4.6 Core Profile | HLSL Shader Model 6.0 → SPIR-V → GLSL 4.60 |
| Direct3D 11 | Feature Level 11.0 | HLSL Shader Model 5.0 |
| Direct3D 12 | Feature Level 11.0 | 支持时使用 Shader Model 6.0，否则使用 5.1 |
| Vulkan | 1.3 | HLSL Shader Model 6.0 → SPIR-V |

## 从源码构建

需要：

- Windows 与 Visual Studio C++ toolchain
- CMake 4.1.2 或更新版本
- vcpkg
- Vulkan SDK

安装 Vulkan SDK 后，请打开新的 **Developer PowerShell for Visual Studio**，以同时加载 MSVC 与 SDK 环境。

```powershell
winget install --id KhronosGroup.VulkanSDK -e
```

根目录 `vcpkg.json` 中声明的依赖项会在 CMake configure 时安装。请指定 vcpkg toolchain，并按需要替换 `C:/vcpkg`。

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

构建会把 `resource`、GLFW 运行时和 `dxcompiler.dll` 复制到可执行文件旁。在 JetBrains Rider 中，请将同一个 `CMAKE_TOOLCHAIN_FILE` 选项加入当前 CMake 配置，并选择 `PmxMod` 目标。

## 命令行

```text
PmxMod [--scene <file.pmscene>] [--renderer <opengl|dx11|dx12|vulkan>]
       [--benchmark <frames>] [--warmup <frames>]
```

使用 `PmxMod --help` 查看语法。benchmark 选项会按指定帧数固定运行，并将性能结果输出到控制台。

## 测试

GoogleTest 会验证 Core 解析与运行时行为、Program 契约、与 API 无关的渲染规划、API descriptor，以及四个后端的 HLSL 编译。

```powershell
cmake -S . -B cmake-build-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DPMXMOD_BUILD_TESTS=ON
cmake --build cmake-build-test
ctest --test-dir cmake-build-test --output-on-failure
```
