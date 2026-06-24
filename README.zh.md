# PmxMod

[English](./README.md) | [한국어](./README.ko.md) | [日本語](./README.ja.md) | **[中文](./README.zh.md)**

PmxMod 是使用 C++23 编写的 PMX/VMD 模型查看器。目前支持 OpenGL、DirectX 11、DirectX 12 和 Vulkan 渲染器。

## 需求

- Windows
- Visual Studio C++ toolchain
- CMake 4.1.2 或更新版本
- vcpkg
- Vulkan SDK

## 渲染器支持

- OpenGL：支持
- DirectX 11：支持
- DirectX 12：支持
- Vulkan：支持

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

使用 VS Code CMake Tools 时：

1. 运行 `CMake: Delete Cache and Reconfigure`。
2. 选择 Release 配置。
3. 构建 `PmxMod` 目标。

如果 vcpkg 路径不是默认位置，请在 VS Code 设置中加入 toolchain 文件：

```json
{
  "cmake.configureSettings": {
    "CMAKE_TOOLCHAIN_FILE": "D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
  }
}
```

## 着色器包

运行时着色器包从 `resource/shaders/` 加载。一个包由 `package.json` manifest 和一个或多个 HLSL effect 组成。默认的 model、edge 和 ground-shadow 着色器位于 `resource/shaders/pmxmod-default`。

后处理示例或模板可以作为单独的包放在 `resource/shaders/` 下。运行时资源由 `SyncResources` 目标从 `resource/` 复制到 CMake 构建目录。

## 备注

- GLFW、GLAD、GLM、Bullet、miniaudio、nlohmann-json、SPIRV-Cross 和 stb 依赖项声明在 `vcpkg.json` 中。
- Vulkan 和 DXC 通过已安装的 Vulkan SDK 查找。
- OpenGL 作为视觉参考渲染器。DirectX 11、DirectX 12 和 Vulkan 会在 API 允许的范围内尽量匹配 model、edge、ground shadow、texture、depth、stencil、blend 和 MSAA 行为。
- DirectX 12 和 Vulkan 使用显式 MSAA render target，然后 resolve 到 swapchain image。它们与 OpenGL default framebuffer 的实现路径不同，但遵循相同的 sample count 策略和最终视觉结果目标。
