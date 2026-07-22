# PmxMod

[English](./README.md) | [한국어](./README.ko.md) | [日本語](./README.ja.md) | **[中文](./README.zh.md)**

PmxMod 是面向 Windows、使用 C++23 编写的 PMX/VMD 查看器与动作播放程序。OpenGL、Direct3D 11、Direct3D 12 和 Vulkan 使用统一的 HLSL 着色器包契约。

## 主要功能

- 加载 PMX 模型以及 VMD 模型／相机动作
- 运行时切换四种渲染 API
- 内置 model、edge 和 ground-shadow 渲染
- 使用 depth、velocity、中间和 history 资源的有序后处理链
- 可安装的 HLSL 着色器包，包含景深、运动模糊和灰度示例
- 加载和保存 `.pmscene` 场景

## 需求

- Windows
- Visual Studio C++ toolchain
- CMake 4.1.2 或更新版本
- vcpkg
- Vulkan SDK
- 支持下列至少一种渲染器的 GPU 与驱动程序

## 渲染器支持

| 渲染器 | 所需版本 | 着色器路径 |
| --- | --- | --- |
| OpenGL | 4.6 Core Profile | HLSL Shader Model 6.0 → SPIR-V → GLSL 4.60 |
| Direct3D 11 | Feature Level 11.0，可用时使用 11.1 | HLSL Shader Model 5.0 |
| Direct3D 12 | 最低 Feature Level 11.0，可检测至 12.2 | Shader Model 6.0，提供 5.1 后备路径 |
| Vulkan | 1.3 | HLSL Shader Model 6.0 → SPIR-V |

## 依赖项

项目使用仓库根目录中的 `vcpkg.json` manifest。你不需要逐个安装库；在 CMake configure 时指定 vcpkg toolchain 文件后，vcpkg 会根据所选 triplet 自动安装 manifest 中的依赖项。

请先安装 Vulkan SDK：

```powershell
winget install --id KhronosGroup.VulkanSDK -e
```

安装 Vulkan SDK 后，请打开新的 PowerShell 窗口，让 CMake 能够读取 SDK 的环境变量。

## 构建

如果 vcpkg 安装在 `C:/vcpkg`：

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

如果 vcpkg 位于其他目录，请替换 toolchain 路径：

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

使用 JetBrains Rider 时：

1. 打开 `Settings | Build, Execution, Deployment | CMake`。
2. 在所选配置的 CMake 选项中添加 vcpkg toolchain 路径。
3. 重新加载 CMake 项目并构建 `PmxMod` 目标。

示例 CMake 选项：

```text
-DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## 着色器包

运行时着色器包从 `resource/shaders/` 加载。一个包由 `package.json` manifest 和一个或多个 HLSL effect 组成。内置的 model、edge 和 ground-shadow 着色器不是包，而是位于 `resource/internal/shaders` 的独立引擎资源。

后处理示例或模板可以作为单独的包放在 `resource/shaders/` 下。运行时资源由 `SyncResources` 目标从 `resource/` 复制到 CMake 构建目录。

## 测试

Viewer 契约测试使用 GoogleTest，普通应用程序构建默认不启用。可使用以下命令启用并运行：

```powershell
cmake -S . -B cmake-build-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DPMXMOD_BUILD_TESTS=ON
cmake --build cmake-build-test
ctest --test-dir cmake-build-test --output-on-failure
```

首批测试不会创建设备，而是验证与 API 无关的后处理规划、参数校验、资源路由、分辨率规则以及 temporal history 状态。

## 备注

- GLFW、GLAD、GLM、Bullet、GoogleTest、miniaudio、nlohmann-json、SPIRV-Cross 和 stb 依赖项声明在 `vcpkg.json` 中。
- Vulkan 和 DXC 通过已安装的 Vulkan SDK 查找。
- OpenGL 作为视觉参考渲染器。DirectX 11、DirectX 12 和 Vulkan 会在 API 允许的范围内尽量匹配 model、edge、ground shadow、texture、depth、stencil、blend 和 MSAA 行为。
- DirectX 12 和 Vulkan 使用显式 MSAA render target，然后 resolve 到 swapchain image。它们与 OpenGL default framebuffer 的实现路径不同，但遵循相同的 sample count 策略和最终视觉结果目标。
