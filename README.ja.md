# PmxMod

[English](./README.md) | [한국어](./README.ko.md) | **[日本語](./README.ja.md)** | [中文](./README.zh.md)

PmxMod は C++23 で書かれた PMX/VMD モデルビューアです。現在は OpenGL、DirectX 11、DirectX 12、Vulkan レンダラーをサポートしています。

## 必要環境

- Windows
- Visual Studio C++ toolchain
- CMake 4.1.2 以降
- vcpkg
- Vulkan SDK

## レンダラー対応

- OpenGL: 対応
- DirectX 11: 対応
- DirectX 12: 対応
- Vulkan: 対応

## 依存関係

リポジトリ直下の `vcpkg.json` manifest を使用します。ライブラリを個別にインストールする必要はありません。CMake の configure 時に vcpkg toolchain ファイルを指定すると、選択された triplet 用の依存関係が自動でインストールされます。

先に Vulkan SDK をインストールしてください。

```powershell
winget install --id KhronosGroup.VulkanSDK -e
```

Vulkan SDK をインストールした後は、新しい PowerShell ウィンドウを開いて CMake が SDK の環境変数を認識できるようにしてください。

## ビルド

vcpkg が `C:/vcpkg` にある場合:

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

vcpkg が別の場所にある場合は、toolchain のパスを置き換えてください。

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

VS Code の CMake Tools を使う場合:

1. `CMake: Delete Cache and Reconfigure` を実行します。
2. Release 構成を選択します。
3. `PmxMod` ターゲットをビルドします。

vcpkg の場所が標準と異なる場合は、VS Code 設定に toolchain ファイルを追加してください。

```json
{
  "cmake.configureSettings": {
    "CMAKE_TOOLCHAIN_FILE": "D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake"
  }
}
```

## シェーダーパッケージ

実行時のシェーダーパッケージは `resource/shaders/` から読み込まれます。パッケージは `package.json` manifest と 1 つ以上の HLSL effect で構成されます。標準の model、edge、ground-shadow シェーダーは `resource/shaders/pmxmod-default` にあります。

ポストプロセスのサンプルや雛形は、`resource/shaders/` 以下に別パッケージとして追加できます。実行時リソースは `SyncResources` ターゲットによって `resource/` から CMake のビルドディレクトリへコピーされます。

## メモ

- GLFW、GLAD、GLM、Bullet、miniaudio、nlohmann-json、SPIRV-Cross、stb は `vcpkg.json` に宣言されています。
- Vulkan と DXC はインストール済みの Vulkan SDK から検出します。
- OpenGL を表示基準のレンダラーとして使います。DirectX 11、DirectX 12、Vulkan は API が許す範囲で model、edge、ground shadow、texture、depth、stencil、blend、MSAA の挙動を OpenGL に合わせます。
- DirectX 12 と Vulkan は明示的な MSAA render target に描画し、swapchain image に resolve します。OpenGL の default framebuffer とは実装経路が異なりますが、sample count 方針と最終表示結果を合わせる方向です。
