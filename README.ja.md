# PmxMod

[English](./README.md) | [한국어](./README.ko.md) | **[日本語](./README.ja.md)** | [中文](./README.zh.md)

PmxMod は PMX モデルを読み込み、VMD のモデル／カメラモーションを再生し、HLSL ポストプロセス効果を適用する Windows 向け C++23 アプリケーションです。同じシーンおよびシェーダーパッケージ契約を OpenGL、Direct3D 11、Direct3D 12、Vulkan で使用します。

## 主な機能

- PMX モデルの読み込みと VMD モデル／カメラモーションの再生
- OpenGL、Direct3D 11、Direct3D 12、Vulkan の実行時切り替え
- 内蔵 model、edge、ground shadow のオン／オフ
- depth、velocity、中間リソース、temporal history を使用する順序付きポストプロセス
- フォルダー単位の HLSL シェーダーパッケージ
- `.pmscene` シーンの読み込みと保存
- タイムライン、補間曲線、音声波形、情報、再生パネル

## 基本的な使い方

1. `PmxMod.exe` と `resource` フォルダーを同じ場所に置いて実行します。
2. **File > New** または **File > Open** でシーンを用意します。
3. Model パネルで PMX モデルを追加し、各モデル行から VMD モーションを指定します。
4. Camera パネルでカメラ VMD を追加し、使用するポストプロセス効果を選択します。
5. Renderer メニューで API を選び、Playback パネルで再生を操作します。

初回起動時の言語は Windows の表示言語に従います。**View > Language** から変更できます。

## 対応ファイル

| ファイル | 用途 |
| --- | --- |
| `.pmx` | MMD モデル |
| `.vmd` | モデルまたはカメラモーション |
| `.pmscene` | モデル、モーション、カメラ、音楽のパスとモデル倍率を保存する PmxMod シーン |

`.pmscene` は外部アセットのパスだけを保存し、ファイル自体は埋め込みません。保存されたパスから元のファイルを読み込める状態にしてください。

## シェーダーパッケージ

シェーダーパッケージのフォルダー全体を次の場所に配置し、PmxMod を再起動します。

```text
PmxMod.exe
resource/
└─ shaders/
   └─ package-directory/
      └─ package.json
```

有効な各 effect は Camera パネルに 1 行ずつ表示されます。複数の effect を同時に有効にでき、一覧の順番で適用されます。付属パッケージには被写界深度、depth 表示、モーションブラー、グレースケールが含まれます。

`resource/internal/shaders` の model、edge、ground-shadow シェーダーは、インストール可能なパッケージではなくエンジンリソースです。schema と HLSL binding は [シェーダーパッケージ契約（韓国語）](./resource/shaders/README.ko.md)を参照してください。

## レンダラー要件

| レンダラー | 最小要件 | シェーダー経路 |
| --- | --- | --- |
| OpenGL | 4.6 Core Profile | HLSL Shader Model 6.0 → SPIR-V → GLSL 4.60 |
| Direct3D 11 | Feature Level 11.0 | HLSL Shader Model 5.0 |
| Direct3D 12 | Feature Level 11.0 | 対応時は Shader Model 6.0、それ以外は 5.1 |
| Vulkan | 1.3 | HLSL Shader Model 6.0 → SPIR-V |

## ソースからのビルド

必要なもの:

- Windows と Visual Studio C++ toolchain
- CMake 4.1.2 以降
- vcpkg
- Vulkan SDK

Vulkan SDK をインストールした後、MSVC と SDK の環境を利用できる新しい **Developer PowerShell for Visual Studio** を開きます。

```powershell
winget install --id KhronosGroup.VulkanSDK -e
```

リポジトリ直下の `vcpkg.json` に宣言された依存関係は CMake configure 時にインストールされます。vcpkg toolchain を指定し、必要に応じて `C:/vcpkg` を変更してください。

```powershell
cmake -S . -B cmake-build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build cmake-build-release --target PmxMod
```

ビルド時に `resource`、GLFW ランタイム、`dxcompiler.dll` が実行ファイルの横へコピーされます。JetBrains Rider では、同じ `CMAKE_TOOLCHAIN_FILE` オプションを有効な CMake プロファイルに追加し、`PmxMod` ターゲットを選択します。

## コマンドライン

```text
PmxMod [--scene <file.pmscene>] [--renderer <opengl|dx11|dx12|vulkan>]
       [--benchmark <frames>] [--warmup <frames>]
```

`PmxMod --help` で構文を確認できます。benchmark オプションは指定フレーム数の固定実行結果をコンソールへ出力します。

## テスト

GoogleTest は Core の解析とランタイム動作、Program 契約、API 非依存の描画計画、API descriptor、4 バックエンド向け HLSL コンパイルを検証します。

```powershell
cmake -S . -B cmake-build-test -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DPMXMOD_BUILD_TESTS=ON
cmake --build cmake-build-test
ctest --test-dir cmake-build-test --output-on-failure
```
