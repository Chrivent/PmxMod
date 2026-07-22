# PmxMod

[English](./README.md) | [한국어](./README.ko.md) | **[日本語](./README.ja.md)** | [中文](./README.zh.md)

PmxMod は Windows 向けの C++23 PMX/VMD ビューアーおよびモーション再生アプリケーションです。OpenGL、Direct3D 11、Direct3D 12、Vulkan で共通の HLSL シェーダーパッケージ契約を使用します。

## 主な機能

- PMX モデルと VMD モデル／カメラモーションの読み込み
- 4 種類のレンダリング API の実行時切り替え
- 内蔵の model、edge、ground-shadow レンダリング
- depth、velocity、中間、history リソースを使用する順序付きポストプロセスチェーン
- 被写界深度、モーションブラー、グレースケールの例を含むインストール可能な HLSL シェーダーパッケージ
- `.pmscene` シーンの読み込みと保存

## 必要環境

- Windows
- Visual Studio C++ toolchain
- CMake 4.1.2 以降
- vcpkg
- Vulkan SDK
- 下記レンダラーのうち 1 つ以上をサポートする GPU とドライバー

## レンダラー対応

| レンダラー | 必要バージョン | シェーダー経路 |
| --- | --- | --- |
| OpenGL | 4.6 Core Profile | HLSL Shader Model 6.0 → SPIR-V → GLSL 4.60 |
| Direct3D 11 | Feature Level 11.0、利用可能な場合は 11.1 | HLSL Shader Model 5.0 |
| Direct3D 12 | 最小 Feature Level 11.0、最大 12.2 まで検出 | Shader Model 6.0、5.1 フォールバック |
| Vulkan | 1.3 | HLSL Shader Model 6.0 → SPIR-V |

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

JetBrains Rider を使う場合:

1. `Settings | Build, Execution, Deployment | CMake` を開きます。
2. 使用するプロファイルの CMake オプションに vcpkg toolchain のパスを追加します。
3. CMake プロジェクトを再読み込みし、`PmxMod` ターゲットをビルドします。

CMake オプションの例:

```text
-DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
```

## シェーダーパッケージ

実行時のシェーダーパッケージは `resource/shaders/` から読み込まれます。パッケージは `package.json` manifest と 1 つ以上の HLSL effect で構成されます。内蔵の model、edge、ground-shadow シェーダーはパッケージではなく、単独のエンジンリソースとして `resource/internal/shaders` にあります。

ポストプロセスのサンプルや雛形は、`resource/shaders/` 以下に別パッケージとして追加できます。実行時リソースは `SyncResources` ターゲットによって `resource/` から CMake のビルドディレクトリへコピーされます。

## テスト

Viewer 契約テストは GoogleTest を使用し、通常のアプリケーションビルドでは無効です。次のコマンドで有効化して実行できます。

```powershell
cmake -S . -B cmake-build-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DPMXMOD_BUILD_TESTS=ON
cmake --build cmake-build-test
ctest --test-dir cmake-build-test --output-on-failure
```

最初のテストスイートは GPU デバイスを生成せず、API 非依存のポストプロセス計画、パラメータ検証、リソース経路、解像度規則、temporal history 状態を検証します。

## メモ

- GLFW、GLAD、GLM、Bullet、GoogleTest、miniaudio、nlohmann-json、SPIRV-Cross、stb は `vcpkg.json` に宣言されています。
- Vulkan と DXC はインストール済みの Vulkan SDK から検出します。
- OpenGL を表示基準のレンダラーとして使います。DirectX 11、DirectX 12、Vulkan は API が許す範囲で model、edge、ground shadow、texture、depth、stencil、blend、MSAA の挙動を OpenGL に合わせます。
- DirectX 12 と Vulkan は明示的な MSAA render target に描画し、swapchain image に resolve します。OpenGL の default framebuffer とは実装経路が異なりますが、sample count 方針と最終表示結果を合わせる方向です。
