#define GLFW_EXPOSE_NATIVE_WIN32
#include "Program/Program.h"

#include "Core/Animation/Camera/CameraAnimation.h"
#include "Core/Animation/Model/Animation.h"
#include "Core/Animation/Model/AnimationBuilder.h"
#include "Core/Model/ModelLoader.h"
#include "Core/Model/ModelAnimator.h"
#include "Core/Model/ModelPose.h"
#include "Core/Parser/BinaryReader.h"
#include "Core/Parser/VmdParser.h"
#include "Viewer/Glfw/GlfwViewer.h"
#include "Viewer/Vulkan/VulkanViewer.h"
#include "Viewer/Dx11/Dx11Viewer.h"
#include "Viewer/Dx12/Dx12Viewer.h"
#include "Util.h"
#include "Program/Language.h"

#include <CommCtrl.h>
#include <GLFW/glfw3native.h>
#include <algorithm>
#include <cwchar>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>

namespace Chrivent {
    void Program::PrintUsage() {
        std::wcout
            << L"PmxMod [--scene <file.pms>] [--renderer <opengl|dx11|dx12|vulkan>]\n"
            << L"       [--benchmark <frames>] [--warmup <frames>]\n";
    }

    bool Program::ParseRenderer(const std::wstring& value, RendererType& rendererType) {
        if (value == L"opengl")
            rendererType = RendererType::OpenGL;
        else if (value == L"dx11")
            rendererType = RendererType::DirectX11;
        else if (value == L"dx12")
            rendererType = RendererType::DirectX12;
        else if (value == L"vulkan")
            rendererType = RendererType::Vulkan;
        else
            return false;
        return true;
    }

    bool Program::ParseCount(const wchar_t* value, std::size_t& count) {
        wchar_t* end = nullptr;
        const unsigned long long parsed = std::wcstoull(value, &end, 10);
        if (!value[0] || !end || *end != L'\0')
            return false;
        count = parsed;
        return true;
    }

    bool Program::ParseArguments(const int argumentCount, wchar_t* arguments[], ProgramOptions& options) {
        for (int index = 1; index < argumentCount; index++) {
            const std::wstring argument = arguments[index];
            if (index + 1 >= argumentCount)
                return false;
            if (argument == L"--scene")
                options.scenePath = arguments[++index];
            else if (argument == L"--renderer") {
                if (!ParseRenderer(arguments[++index], options.rendererType))
                    return false;
            } else if (argument == L"--benchmark") {
                if (!ParseCount(arguments[++index], options.benchmarkFrames)
                    || options.benchmarkFrames == 0)
                    return false;
            } else if (argument == L"--warmup") {
                if (!ParseCount(arguments[++index], options.warmupFrames))
                    return false;
            } else
                return false;
        }
        return true;
    }

    const char* Program::ResolveRendererName(const RendererType rendererType) {
        switch (rendererType) {
            case RendererType::OpenGL: return "opengl";
            case RendererType::DirectX11: return "dx11";
            case RendererType::DirectX12: return "dx12";
            case RendererType::Vulkan: return "vulkan";
        }
        return "unknown";
    }

    LRESULT CALLBACK Program::ViewerWindowProc(
        const HWND hwnd,
        const UINT msg,
        const WPARAM wParam,
        const LPARAM lParam,
        const UINT_PTR subclassId,
        const DWORD_PTR data) {
        auto* program = reinterpret_cast<Program*>(data);
        if (!program || subclassId != kViewerWindowSubclassId)
            return DefSubclassProc(hwnd, msg, wParam, lParam);
        switch (msg) {
            case WM_ENTERSIZEMOVE:
            case WM_ENTERMENULOOP:
                SetTimer(hwnd, kViewerModalFrameTimerId, 16, nullptr);
                break;
            case WM_EXITSIZEMOVE:
            case WM_EXITMENULOOP:
                KillTimer(hwnd, kViewerModalFrameTimerId);
                break;
            case WM_TIMER:
                if (wParam == kViewerModalFrameTimerId) {
                    if (!program->RenderViewerModalFrame())
                        PostMessageW(hwnd, WM_CLOSE, 0, 0);
                    return 0;
                }
                break;
            case WM_NCDESTROY:
                KillTimer(hwnd, kViewerModalFrameTimerId);
                RemoveWindowSubclass(hwnd, ViewerWindowProc, kViewerWindowSubclassId);
                if (program->viewerNativeWindow == hwnd)
                    program->viewerNativeWindow = nullptr;
                break;
            default:
                break;
        }
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    void Program::CreateViewer(const RendererType rendererType) {
        switch (rendererType) {
            case RendererType::OpenGL:
                viewer = std::make_unique<GlfwViewer>();
                break;
            case RendererType::DirectX11:
                viewer = std::make_unique<Dx11Viewer>();
                break;
            case RendererType::DirectX12:
                viewer = std::make_unique<Dx12Viewer>();
                break;
            case RendererType::Vulkan:
                viewer = std::make_unique<VulkanViewer>();
                break;
        }
        currentRendererType = rendererType;
    }

    bool Program::InitializeViewer() {
        if (!viewer)
            return false;
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW.\n";
            return false;
        }
        glfwDefaultWindowHints();
        viewer->ConfigureGlfwHints();
        viewer->window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
        if (!viewer->window) {
            std::cerr << "Failed to create viewer window.\n";
            viewer.reset();
            glfwTerminate();
            return false;
        }
        InstallViewerWindowSubclass();
        inputManager.AttachWindow(viewer->window);
        PositionViewerOnRightMonitor();
        glfwMaximizeWindow(viewer->window);
        glfwPollEvents();
        glfwGetFramebufferSize(viewer->window, &viewer->screenWidth, &viewer->screenHeight);
        if (viewer->screenWidth <= 0 || viewer->screenHeight <= 0) {
            std::cerr << "Invalid framebuffer size.\n";
            RemoveViewerWindowSubclass();
            viewer.reset();
            glfwTerminate();
            return false;
        }
        if (!viewer->Setup()) {
            std::cerr << "Failed to set up renderer.\n";
            RemoveViewerWindowSubclass();
            viewer.reset();
            glfwTerminate();
            return false;
        }
        if (!viewer->Resize()) {
            RemoveViewerWindowSubclass();
            viewer.reset();
            glfwTerminate();
            return false;
        }
        glfwPollEvents();
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(viewer->window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth != viewer->screenWidth || framebufferHeight != viewer->screenHeight) {
            viewer->screenWidth = framebufferWidth;
            viewer->screenHeight = framebufferHeight;
            if (!viewer->Resize()) {
                RemoveViewerWindowSubclass();
                viewer.reset();
                glfwTerminate();
                return false;
            }
        }
        viewer->CreateFpsOverlay();
        return true;
    }

    void Program::InstallViewerWindowSubclass() {
        if (!viewer || !viewer->window)
            return;
        viewerNativeWindow = glfwGetWin32Window(viewer->window);
        if (viewerNativeWindow)
            SetWindowSubclass(viewerNativeWindow, ViewerWindowProc, kViewerWindowSubclassId, reinterpret_cast<DWORD_PTR>(this));
    }

    void Program::RemoveViewerWindowSubclass() {
        if (!viewerNativeWindow)
            return;
        KillTimer(viewerNativeWindow, kViewerModalFrameTimerId);
        RemoveWindowSubclass(viewerNativeWindow, ViewerWindowProc, kViewerWindowSubclassId);
        viewerNativeWindow = nullptr;
    }

    bool Program::RenderViewerModalFrame() {
        if (viewerModalFrameActive)
            return true;
        viewerModalFrameActive = true;
        const bool frameResult = RunFrame(nullptr, false);
        viewerModalFrameActive = false;
        return frameResult;
    }

    void Program::PositionViewerOnRightMonitor() const {
        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        if (!monitors || monitorCount < 2)
            return;
        int firstX = 0;
        int firstY = 0;
        int firstWidth = 0;
        int firstHeight = 0;
        glfwGetMonitorWorkarea(monitors[0], &firstX, &firstY, &firstWidth, &firstHeight);
        int leftmostX = firstX;
        int rightX = firstX;
        int rightY = firstY;
        int rightWidth = firstWidth;
        int rightHeight = firstHeight;
        bool hasDistinctWorkArea = false;
        for (int index = 1; index < monitorCount; index++) {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            glfwGetMonitorWorkarea(monitors[index], &x, &y, &width, &height);
            hasDistinctWorkArea = hasDistinctWorkArea
                || x != firstX || y != firstY || width != firstWidth || height != firstHeight;
            leftmostX = std::min(leftmostX, x);
            if (x > rightX) {
                rightX = x;
                rightY = y;
                rightWidth = width;
                rightHeight = height;
            }
        }
        if (!hasDistinctWorkArea || rightX <= leftmostX)
            return;
        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(viewer->window, &windowWidth, &windowHeight);
        const int x = rightX + std::max(0, (rightWidth - windowWidth) / 2);
        const int y = rightY + std::max(0, (rightHeight - windowHeight) / 2);
        glfwSetWindowPos(viewer->window, x, y);
    }

    bool Program::ChangeRenderer(const RendererType rendererType) {
        if (rendererType == currentRendererType)
            return true;
        const int playbackFrame = viewer ? viewer->animTime * 30.0f + 0.5f : 0;
        GLFWwindow* previousWindow = viewer ? viewer->window : nullptr;
        if (viewer)
            viewer->WaitIdle();
        RemoveViewerWindowSubclass();
        ClearInstances();
        viewer.reset();
        if (previousWindow) {
            glfwMakeContextCurrent(nullptr);
            glfwDestroyWindow(previousWindow);
        }
        CreateViewer(rendererType);
        if (!InitializeViewer())
            return false;
        LoadSelectedShaderEffect();
        saveTime = std::chrono::steady_clock::now();
        if (!LoadScene(panelManager.GetSceneConfig(), false))
            return false;
        panelManager.BindSound(music);
        cameraManager.SeekFrame(*viewer, music, playbackFrame, saveTime);
        ResetPhysics(playbackFrame);
        cameraManager.UpdateCamera(*viewer);
        return true;
    }

    void Program::Shutdown() {
        GLFWwindow* window = viewer ? viewer->window : nullptr;
        if (viewer)
            viewer->WaitIdle();
        RemoveViewerWindowSubclass();
        ClearInstances();
        music.Stop();
        panelManager.DestroyGui();
        viewer.reset();
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
    }

    void Program::DiscoverShaderPackages() {
        shaderPackages.clear();
        if (!viewer)
            return;
        auto [packages, errors] = ShaderPackageLoader::Discover(viewer->ResolveShaderPackagesDirectory());
        shaderPackages = std::move(packages);
        for (const auto& error : errors)
            std::cerr << "Failed to load shader package: " << error << '\n';
        std::size_t effectCount = 0;
        for (const auto& package : shaderPackages) {
            effectCount += std::ranges::count(package.effects, EffectType::PostProcess, &EffectDefinition::type);
        }
        std::cout << "shader_packages=" << shaderPackages.size() << '\n';
        std::cout << "effects=" << effectCount << '\n';
        selectedShaderEffectIndex = effectCount == 0 ? 0 : std::min(selectedShaderEffectIndex, effectCount - 1);
        if (shaderEffectEnabled.size() != effectCount) {
            std::vector newEnabled(effectCount, false);
            const size_t copyCount = std::min(shaderEffectEnabled.size(), newEnabled.size());
            for (size_t index = 0; index < copyCount; index++)
                newEnabled[index] = shaderEffectEnabled[index];
            if (effectCount > 0 && std::ranges::none_of(newEnabled, [](const bool enabled) { return enabled; }))
                newEnabled[selectedShaderEffectIndex] = true;
            shaderEffectEnabled = std::move(newEnabled);
        }
        UpdateShaderPanel();
        LoadSelectedShaderEffect();
    }

    void Program::UpdateShaderPanel() {
        std::vector<std::wstring> shaderNames;
        for (const auto& package : shaderPackages) {
            for (const auto& effect : package.effects) {
                if (effect.type != EffectType::PostProcess)
                    continue;
                shaderNames.emplace_back(
                    Util::Utf8ToWString(package.name) + L" / " + Util::Utf8ToWString(effect.name));
            }
        }
        panelManager.ApplyShaderNames(shaderNames, selectedShaderEffectIndex, shaderEffectEnabled);
    }

    void Program::LoadSelectedShaderEffect() const {
        if (!viewer)
            return;
        if (shaderEffectEnabled.empty() || std::ranges::none_of(shaderEffectEnabled, [](const bool enabled) { return enabled; })) {
            viewer->ClearPostProcessEffect();
            return;
        }
        const size_t activeShaderEffectIndex =
            selectedShaderEffectIndex < shaderEffectEnabled.size() && shaderEffectEnabled[selectedShaderEffectIndex]
                ? selectedShaderEffectIndex
                : static_cast<size_t>(std::ranges::find(shaderEffectEnabled, true) - shaderEffectEnabled.begin());
        size_t effectIndex = 0;
        for (const auto& package : shaderPackages) {
            for (const auto& effect : package.effects) {
                if (effect.type != EffectType::PostProcess)
                    continue;
                if (effectIndex++ != activeShaderEffectIndex)
                    continue;
                if (viewer->LoadPostProcessEffect(effect))
                    std::cout << "active_effect=" << package.id << ':' << effect.id << '\n';
                return;
            }
        }
    }

    bool Program::LoadScene(const SceneConfig& sceneConfig, const bool resetPlaybackRange) {
        music.Pause();
        std::vector<std::unique_ptr<Instance>> loadedInstances;
        if (!LoadInstances(sceneConfig, loadedInstances)) {
            std::cerr << "Failed to load scene instances.\n";
            return false;
        }
        ClearInstances();
        instances = std::move(loadedInstances);
        music.Stop();
        if (!sceneConfig.musicPath.empty() && music.Init(sceneConfig.musicPath, false))
            music.Pause();
        panelManager.BindSound(music);
        cameraManager.LoadCameraAnim(sceneConfig.cameraAnim);
        panelManager.ApplyMotionMode(MotionTimelineMode::Camera);
        viewer->elapsed = 0.0f;
        viewer->animTime = 0.0f;
        viewer->skipPhysics = false;
        saveTime = std::chrono::steady_clock::now();
        cameraManager.Stop(*viewer, music, saveTime);
        panelManager.UpdateFrameLimits(CalculatePlaybackLastFrame(), CalculateMotionLastFrame(), resetPlaybackRange);
        panelManager.ApplyCameraMotionPath(sceneConfig.cameraAnim);
        const int startFrame = panelManager.GetPlaybackFrameRange().start;
        if (startFrame > 0) {
            cameraManager.SeekFrame(*viewer, music, startFrame, saveTime);
            ResetPhysics(startFrame);
        }
        return true;
    }

    bool Program::LoadInstances(const SceneConfig& sceneConfig, std::vector<std::unique_ptr<Instance>>& loadedInstances) const {
        loadedInstances.clear();
        loadedInstances.reserve(sceneConfig.modelConfigs.size());
        for (const auto& [modelPath, animPaths, scale] : sceneConfig.modelConfigs) {
            auto instance = viewer->CreateInstance();
            const auto pmxModel = std::make_shared<Model>();
            const ModelLoader loader(*pmxModel);
            if (!loader.Load(modelPath, viewer->pmxDir)) {
                std::cerr << "Failed to load pmx file.\n";
                return false;
            }
            instance->model = pmxModel;
            const ModelAnimator animator(*instance->model);
            animator.InitializeAnimation();
            AnimationBuilder animationBuilder(instance->model);
            for (const auto& vmdPath : animPaths) {
                VmdParser vmd;
                const auto parseResult = vmd.ReadFile(vmdPath);
                if (!parseResult) {
                    std::cerr << "Failed to read VMD file: " << BinaryReader::FormatParseError(parseResult.error()) << '\n';
                    return false;
                }
                animationBuilder.Build(vmd.GetData());
            }
            auto vmdAnim = animationBuilder.TakeAnimation();
            animator.SyncPhysics(*vmdAnim, 0.0f);
            instance->anim = std::move(vmdAnim);
            instance->scale = scale;
            if (!instance->Setup(*viewer))
                return false;
            loadedInstances.emplace_back(std::move(instance));
        }
        return true;
    }

    int Program::CalculateMotionLastFrame() const {
        int lastFrame = cameraManager.CalculateLastFrame();
        for (const auto& instance : instances) {
            if (instance && instance->anim) {
                const uint32_t animationLastFrame = instance->anim->CalculateLastFrame();
                const int timelineLastFrame = std::min(
                    animationLastFrame, static_cast<uint32_t>(std::numeric_limits<int>::max()));
                lastFrame = std::max(lastFrame, timelineLastFrame);
            }
        }
        return lastFrame;
    }

    int Program::CalculatePlaybackLastFrame() const {
        int lastFrame = CalculateMotionLastFrame();
        if (music.HasSound())
            lastFrame = std::max(lastFrame, static_cast<int>(std::ceil(music.GetLengthSeconds() * 30.0)));
        return lastFrame;
    }

    void Program::ResetPhysics(const int frame) const {
        for (const auto& instance : instances) {
            if (!instance || !instance->model || !instance->anim)
                continue;
            const ModelAnimator animator(*instance->model);
            const ModelPose pose(*instance->model);
            animator.BeginAnimation();
            instance->anim->Evaluate(frame);
            animator.UpdateMorphAnimation();
            pose.UpdateNodeAnimation(false);
            pose.UpdateNodeAnimation(true);
            pose.ResetPhysics();
            animator.SyncPhysics(*instance->anim, frame);
        }
    }

    void Program::UpdateMotionPanel(const size_t modelIndex) {
        if (modelIndex >= instances.size() || !instances[modelIndex] || !instances[modelIndex]->model)
            return;
        const auto& instance = *instances[modelIndex];
        const auto& model = *instance.model;
        const auto NormalizeKeys = [](std::vector<MotionTimelineKey>& keys) {
            std::ranges::sort(keys, {}, &MotionTimelineKey::frame);
            std::vector<MotionTimelineKey> normalized;
            normalized.reserve(keys.size());
            for (auto& key : keys) {
                if (!normalized.empty() && normalized.back().frame == key.frame) {
                    if (normalized.back().curves.empty() && !key.curves.empty()) {
                        normalized.back().curves = std::move(key.curves);
                        normalized.back().values = std::move(key.values);
                    }
                    continue;
                }
                normalized.emplace_back(std::move(key));
            }
            keys = std::move(normalized);
        };
        const auto CollectFrames = [](const std::vector<MotionTimelineRow>& rows) {
            std::vector<int> frames;
            for (const auto& row : rows) {
                for (const auto& key : row.keys)
                    frames.emplace_back(key.frame);
            }
            std::ranges::sort(frames);
            const auto uniqueFrames = std::ranges::unique(frames);
            frames.erase(uniqueFrames.begin(), uniqueFrames.end());
            return frames;
        };
        const auto ToTimelineFrame = [](const uint32_t frame) {
            constexpr uint32_t maxFrame = std::numeric_limits<int>::max();
            return frame > maxFrame ? std::numeric_limits<int>::max() : static_cast<int>(frame);
        };
        std::unordered_map<const Node*, std::vector<MotionTimelineKey>> nodeKeys;
        std::unordered_map<const IkSolver*, std::vector<MotionTimelineKey>> ikKeys;
        std::unordered_map<const Morph*, std::vector<MotionTimelineKey>> morphKeys;
        if (instance.anim) {
            for (const auto& [node, keys] : instance.anim->nodeTracks) {
                auto& timelineKeys = nodeKeys[node.get()];
                timelineKeys.reserve(keys.size());
                for (const auto& [frame, translate, rotate, txBezier, tyBezier, tzBezier, rotBezier] : keys) {
                    glm::quat rotation = glm::normalize(rotate);
                    if (rotation.w < 0.0f)
                        rotation = -rotation;
                    timelineKeys.push_back({
                        .frame = ToTimelineFrame(frame),
                        .curves = {
                            txBezier.GetControlPoints(),
                            tyBezier.GetControlPoints(),
                            tzBezier.GetControlPoints(),
                            rotBezier.GetControlPoints()
                        },
                        .values = {
                            translate.x,
                            translate.y,
                            translate.z,
                            glm::degrees(glm::angle(rotation))
                        }
                    });
                }
            }
            for (const auto& [ikSolver, keys] : instance.anim->ikTracks) {
                auto& timelineKeys = ikKeys[ikSolver.get()];
                timelineKeys.reserve(keys.size());
                for (const auto& [frame, ikEnable] : keys)
                    timelineKeys.push_back({.frame = ToTimelineFrame(frame)});
            }
            for (const auto& [morph, keys] : instance.anim->morphTracks) {
                auto& timelineKeys = morphKeys[morph.get()];
                timelineKeys.reserve(keys.size());
                for (const auto& [frame, morphWeight] : keys)
                    timelineKeys.push_back({.frame = ToTimelineFrame(frame)});
            }
        }
        std::vector<MotionTimelineGroup> groups;
        groups.reserve(model.skeletonData.displayFrames.size() + 1);
        const auto& cameraKeys = cameraManager.ResolveAnimationKeys();
        if (!cameraKeys.empty()) {
            MotionTimelineRow cameraRow{
                .name = Language::Text("motion.camera"),
                .curveNames = {
                    Language::Text("interpolation.x"),
                    Language::Text("interpolation.y"),
                    Language::Text("interpolation.z"),
                    Language::Text("interpolation.rotation"),
                    Language::Text("interpolation.distance"),
                    Language::Text("interpolation.fov")
                }
            };
            cameraRow.keys.reserve(cameraKeys.size());
            for (const auto& [frame, interest, rotate, distance, fov
                , ixBezier, iyBezier, izBezier, rotateBezier, distanceBezier, fovBezier] : cameraKeys) {
                cameraRow.keys.push_back({
                    .frame = ToTimelineFrame(frame),
                    .curves = {
                        ixBezier.GetControlPoints(),
                        iyBezier.GetControlPoints(),
                        izBezier.GetControlPoints(),
                        rotateBezier.GetControlPoints(),
                        distanceBezier.GetControlPoints(),
                        fovBezier.GetControlPoints()
                    },
                    .values = {
                        interest.x,
                        interest.y,
                        interest.z,
                        glm::degrees(glm::length(rotate)),
                        distance,
                        glm::degrees(fov)
                    }
                });
            }
            cameraRow.expandable = true;
            MotionTimelineGroup cameraGroup{
                .name = Language::Text("motion.camera"),
                .rows = {std::move(cameraRow)},
                .mode = MotionTimelineMode::Camera,
                .grouped = false
            };
            cameraGroup.keyFrames = CollectFrames(cameraGroup.rows);
            groups.emplace_back(std::move(cameraGroup));
        }
        for (const auto& [name, boneIndices, morphIndices] : model.skeletonData.displayFrames) {
            MotionTimelineGroup group{
                .name = Util::Utf8ToWString(name)
            };
            group.rows.reserve(boneIndices.size() + morphIndices.size());
            for (const uint32_t boneIndex : boneIndices) {
                if (boneIndex >= model.skeletonData.nodes.size())
                    continue;
                const auto& node = model.skeletonData.nodes[boneIndex];
                if (!node)
                    continue;
                auto keys = nodeKeys[node.get()];
                if (const auto ikSolver = node->ikSolver.lock()) {
                    const auto& solverKeys = ikKeys[ikSolver.get()];
                    keys.insert(keys.end(), solverKeys.begin(), solverKeys.end());
                }
                NormalizeKeys(keys);
                group.rows.push_back({
                    .name = Util::Utf8ToWString(node->name),
                    .curveNames = {
                        Language::Text("interpolation.x"),
                        Language::Text("interpolation.y"),
                        Language::Text("interpolation.z"),
                        Language::Text("interpolation.rotation")
                    },
                    .keys = std::move(keys),
                    .expandable = true
                });
            }
            for (const uint32_t morphIndex : morphIndices) {
                if (morphIndex >= model.morphData.morphs.size())
                    continue;
                const auto& morph = model.morphData.morphs[morphIndex];
                if (!morph)
                    continue;
                auto keys = morphKeys[morph.get()];
                NormalizeKeys(keys);
                group.rows.push_back({
                    .name = Util::Utf8ToWString(morph->name),
                    .keys = std::move(keys)
                });
            }
            if (group.rows.empty())
                continue;
            group.keyFrames = CollectFrames(group.rows);
            groups.emplace_back(std::move(group));
        }
        if (groups.size() == (cameraKeys.empty() ? 0 : 1)) {
            MotionTimelineGroup boneGroup{.name = Language::Text("motion.bones")};
            for (const auto& node : model.skeletonData.nodes) {
                if (!node)
                    continue;
                auto keys = nodeKeys[node.get()];
                if (const auto ikSolver = node->ikSolver.lock()) {
                    const auto& solverKeys = ikKeys[ikSolver.get()];
                    keys.insert(keys.end(), solverKeys.begin(), solverKeys.end());
                }
                NormalizeKeys(keys);
                boneGroup.rows.push_back({
                    .name = Util::Utf8ToWString(node->name),
                    .curveNames = {
                        Language::Text("interpolation.x"),
                        Language::Text("interpolation.y"),
                        Language::Text("interpolation.z"),
                        Language::Text("interpolation.rotation")
                    },
                    .keys = std::move(keys),
                    .expandable = true
                });
            }
            boneGroup.keyFrames = CollectFrames(boneGroup.rows);
            if (!boneGroup.rows.empty())
                groups.emplace_back(std::move(boneGroup));
            MotionTimelineGroup morphGroup{.name = Language::Text("motion.morphs")};
            for (const auto& morph : model.morphData.morphs) {
                if (!morph)
                    continue;
                auto keys = morphKeys[morph.get()];
                NormalizeKeys(keys);
                morphGroup.rows.push_back({
                    .name = Util::Utf8ToWString(morph->name),
                    .keys = std::move(keys)
                });
            }
            morphGroup.keyFrames = CollectFrames(morphGroup.rows);
            if (!morphGroup.rows.empty())
                groups.emplace_back(std::move(morphGroup));
        }
        std::wstring modelName = Util::Utf8ToWString(model.infoData.modelName);
        if (modelName.empty() && modelIndex < panelManager.GetSceneConfig().modelConfigs.size())
            modelName = panelManager.GetSceneConfig().modelConfigs[modelIndex].modelPath.filename().wstring();
        panelManager.ApplyMotionTimeline(std::move(modelName), std::move(groups));
    }

    void Program::UpdateCameraMotionPanel() {
        const auto& cameraKeys = cameraManager.ResolveAnimationKeys();
        std::vector<MotionTimelineGroup> groups;
        if (!cameraKeys.empty()) {
            MotionTimelineRow cameraRow{
                .name = Language::Text("motion.camera"),
                .curveNames = {
                    Language::Text("interpolation.x"),
                    Language::Text("interpolation.y"),
                    Language::Text("interpolation.z"),
                    Language::Text("interpolation.rotation"),
                    Language::Text("interpolation.distance"),
                    Language::Text("interpolation.fov")
                },
                .expandable = true
            };
            cameraRow.keys.reserve(cameraKeys.size());
            for (const auto& [frame, interest, rotate, distance, fov
                , ixBezier, iyBezier, izBezier, rotateBezier, distanceBezier, fovBezier] : cameraKeys) {
                cameraRow.keys.push_back({
                    .frame = frame > static_cast<uint32_t>(std::numeric_limits<int>::max())
                        ? std::numeric_limits<int>::max()
                        : static_cast<int>(frame),
                    .curves = {
                        ixBezier.GetControlPoints(),
                        iyBezier.GetControlPoints(),
                        izBezier.GetControlPoints(),
                        rotateBezier.GetControlPoints(),
                        distanceBezier.GetControlPoints(),
                        fovBezier.GetControlPoints()
                    },
                    .values = {
                        interest.x,
                        interest.y,
                        interest.z,
                        glm::degrees(glm::length(rotate)),
                        distance,
                        glm::degrees(fov)
                    }
                });
            }
            std::vector<int> keyFrames;
            keyFrames.reserve(cameraRow.keys.size());
            for (const auto& key : cameraRow.keys)
                keyFrames.emplace_back(key.frame);
            MotionTimelineGroup cameraGroup{
                .name = Language::Text("motion.camera"),
                .rows = {std::move(cameraRow)},
                .keyFrames = std::move(keyFrames),
                .mode = MotionTimelineMode::Camera,
                .grouped = false
            };
            groups.emplace_back(std::move(cameraGroup));
        }
        std::wstring name = panelManager.GetSceneConfig().cameraAnim.empty()
            ? Language::Text("motion.camera")
            : panelManager.GetSceneConfig().cameraAnim.filename().wstring();
        panelManager.ApplyMotionTimeline(std::move(name), std::move(groups));
    }

    void Program::UpdateShaderMotionPanel(const size_t effectIndex) {
        size_t currentIndex = 0;
        for (const auto& package : shaderPackages) {
            for (const auto& effect : package.effects) {
                if (effect.type != EffectType::PostProcess)
                    continue;
                if (currentIndex++ != effectIndex)
                    continue;
                const std::wstring name =
                    Util::Utf8ToWString(package.name) + L" / " + Util::Utf8ToWString(effect.name);
                panelManager.ApplyMotionTimeline(name, {});
                return;
            }
        }
        panelManager.ApplyMotionTimeline(Language::Text("panel.camera"), {});
    }

    void Program::ClearInstances() {
        for (const auto& instance : instances)
            instance->Clear();
        instances.clear();
    }

    bool Program::UpdateFramebufferSize() const {
        int newW = 0;
        int newH = 0;
        glfwGetFramebufferSize(viewer->window, &newW, &newH);
        if (newW == viewer->screenWidth && newH == viewer->screenHeight)
            return true;
        viewer->screenWidth = newW;
        viewer->screenHeight = newH;
        return viewer->Resize();
    }

    void Program::TickFps() {
        fpsFrame++;
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsTime).count();
        if (sec > 1.0) {
            viewer->UpdateFps(fpsFrame / sec);
            fpsFrame = 0;
            fpsTime = std::chrono::steady_clock::now();
        }
    }

    bool Program::RunFrame(FrameTiming* timing, const bool pollGuiWindows) {
        const auto frameStart = std::chrono::steady_clock::now();
        glfwPollEvents();
        if (pollGuiWindows)
            panelManager.PollGuiWindows();
        if (panelManager.ConsumeLanguageDirty()) {
            panelManager.RefreshLanguage();
            return true;
        }
        if (panelManager.ConsumeRendererDirty()) {
            if (!ChangeRenderer(panelManager.GetRendererType()))
                return false;
            return true;
        }
        std::filesystem::path modelPath;
        if (panelManager.ConsumeAddModelPath(modelPath)) {
            SceneConfig sceneConfig = panelManager.GetSceneConfig();
            sceneConfig.modelConfigs.emplace_back(ModelConfig{
                .modelPath = std::move(modelPath),
                .animPaths = {},
                .scale = 1.0f
            });
            if (LoadScene(sceneConfig))
                panelManager.CommitSceneConfig(sceneConfig);
        }
        if (panelManager.ConsumeSceneConfigDirty() && LoadScene(panelManager.GetSceneConfig()))
            panelManager.RefreshModelList();
        std::filesystem::path cameraMotionPath;
        if (panelManager.ConsumeCameraMotionPath(cameraMotionPath)) {
            SceneConfig sceneConfig = panelManager.GetSceneConfig();
            sceneConfig.cameraAnim = std::move(cameraMotionPath);
            if (LoadScene(sceneConfig)) {
                panelManager.CommitSceneConfig(sceneConfig);
                panelManager.ApplyMotionMode(MotionTimelineMode::Camera);
                UpdateCameraMotionPanel();
            }
        }
        if (panelManager.ConsumeDeleteCameraMotion()) {
            SceneConfig sceneConfig = panelManager.GetSceneConfig();
            sceneConfig.cameraAnim.clear();
            if (LoadScene(sceneConfig)) {
                panelManager.CommitSceneConfig(sceneConfig);
                panelManager.ApplyMotionMode(MotionTimelineMode::Camera);
                UpdateCameraMotionPanel();
            }
        }
        size_t deleteModelIndex = 0;
		if (panelManager.ConsumeDeleteModelIndex(deleteModelIndex)) {
			SceneConfig sceneConfig = panelManager.GetSceneConfig();
			if (deleteModelIndex < sceneConfig.modelConfigs.size()) {
				sceneConfig.modelConfigs.erase(sceneConfig.modelConfigs.begin() + static_cast<std::ptrdiff_t>(deleteModelIndex));
				if (LoadScene(sceneConfig)) {
                    panelManager.CommitSceneConfig(sceneConfig);
                    if (!sceneConfig.modelConfigs.empty())
                        UpdateMotionPanel(std::min(deleteModelIndex, sceneConfig.modelConfigs.size() - 1));
                    else
                        UpdateCameraMotionPanel();
				}
			}
		}
		size_t motionModelIndex = 0;
		std::filesystem::path modelMotionPath;
		if (panelManager.ConsumeModelMotionPath(motionModelIndex, modelMotionPath)) {
			SceneConfig sceneConfig = panelManager.GetSceneConfig();
			if (motionModelIndex < sceneConfig.modelConfigs.size()) {
				sceneConfig.modelConfigs[motionModelIndex].animPaths = {std::move(modelMotionPath)};
				if (LoadScene(sceneConfig)) {
					panelManager.CommitSceneConfig(sceneConfig);
					panelManager.ApplyMotionMode(MotionTimelineMode::Model);
					UpdateMotionPanel(motionModelIndex);
				}
			}
		}
		size_t selectedModelIndex = 0;
		if (panelManager.ConsumeSelectedModelIndex(selectedModelIndex)) {
			panelManager.ApplyMotionMode(MotionTimelineMode::Model);
			UpdateMotionPanel(selectedModelIndex);
		}
		if (panelManager.ConsumeCameraMotionSelected()) {
			panelManager.ApplyMotionMode(MotionTimelineMode::Camera);
			UpdateCameraMotionPanel();
		}
		size_t selectedEffectIndex = 0;
        bool selectedEffectEnabled = false;
        if (panelManager.ConsumeSelectedShaderIndex(selectedEffectIndex, selectedEffectEnabled)) {
            selectedShaderEffectIndex = selectedEffectIndex;
            if (selectedShaderEffectIndex >= shaderEffectEnabled.size())
                shaderEffectEnabled.resize(selectedShaderEffectIndex + 1, false);
            shaderEffectEnabled[selectedShaderEffectIndex] = selectedEffectEnabled;
            LoadSelectedShaderEffect();
            panelManager.ApplyMotionMode(MotionTimelineMode::Camera);
            UpdateShaderMotionPanel(selectedShaderEffectIndex);
        }
        int seekFrame = 0;
        bool seekFinished = false;
        if (panelManager.ConsumeSeekFrame(seekFrame, seekFinished)) {
            viewer->skipPhysics = !seekFinished;
            cameraManager.SeekFrame(*viewer, music, seekFrame, saveTime);
            if (seekFinished) {
                ResetPhysics(seekFrame);
                viewer->skipPhysics = false;
            }
        }
        switch (panelManager.ConsumePlaybackCommand()) {
            case PlaybackCommand::Play:
                viewer->skipPhysics = false;
                if (const auto [start, end] = panelManager.GetPlaybackFrameRange();
                    viewer->animTime * 30.0f < start ||
                    viewer->animTime * 30.0f >= end) {
                    cameraManager.SeekFrame(*viewer, music, start, saveTime);
                    ResetPhysics(start);
                }
                cameraManager.Play(music);
                break;
            case PlaybackCommand::Pause:
                cameraManager.Pause(music);
                break;
            case PlaybackCommand::Stop:
                viewer->skipPhysics = false;
                cameraManager.Stop(*viewer, music, saveTime);
                cameraManager.SeekFrame(*viewer, music, 0, saveTime);
                ResetPhysics(0);
                break;
            case PlaybackCommand::None:
                break;
        }
        cameraManager.ApplyMotionCameraState(panelManager.IsCameraMode());
        inputManager.Update(*viewer);
        cameraManager.HandleInput(inputManager, *viewer, music);
        if (!UpdateFramebufferSize())
            return false;
        if (benchmarkMode) {
            viewer->elapsed = 1.0f / 30.0f;
            viewer->animTime += viewer->elapsed;
        } else
            cameraManager.StepTime(*viewer, music, saveTime);
        const int endFrame = panelManager.GetPlaybackFrameRange().end;
        const float playbackFrame = viewer->animTime * 30.0f;
        if (cameraManager.IsPlaying() && playbackFrame >= endFrame) {
            if (panelManager.IsPlaybackRepeatEnabled()) {
                const int startFrame = panelManager.GetPlaybackFrameRange().start;
                cameraManager.SeekFrame(*viewer, music, startFrame, saveTime);
                ResetPhysics(startFrame);
            } else {
                cameraManager.SeekFrame(*viewer, music, endFrame, saveTime);
                cameraManager.Pause(music);
                ResetPhysics(endFrame);
            }
            viewer->skipPhysics = false;
        }
        panelManager.ApplyPlaybackState(cameraManager.IsPlaying());
        panelManager.SetPlaybackFrame(viewer->animTime * 30.0f + 0.5f);
        cameraManager.UpdateCamera(*viewer);
        viewer->UpdateFpsVisibility(panelManager.IsFpsVisible());
        const auto animationStart = std::chrono::steady_clock::now();
        if (timing) {
            modelUpdateTimings.clear();
            modelUpdateTimings.resize(instances.size());
        }
        const bool physicsEnabled = panelManager.IsPhysicsEnabled();
        taskExecutor.Run(instances.size(), [&](const std::size_t index) {
            instances[index]->PrepareUpdate(*viewer, physicsEnabled, timing ? &modelUpdateTimings[index] : nullptr);
        });
        const auto animationEnd = std::chrono::steady_clock::now();
        skinningTaskOffsets.resize(instances.size() + 1);
        skinningTaskOffsets[0] = 0;
        for (std::size_t index = 0; index < instances.size(); index++) {
            skinningTaskOffsets[index + 1] =
                skinningTaskOffsets[index] + instances[index]->CalculateSkinningTaskCount();
        }
        taskExecutor.Run(skinningTaskOffsets.back(), [&](const std::size_t taskIndex) {
            const auto offset = std::ranges::upper_bound(skinningTaskOffsets, taskIndex);
            const std::size_t instanceIndex = std::distance(skinningTaskOffsets.begin(), offset) - 1;
            instances[instanceIndex]->UpdateSkinning(taskIndex - skinningTaskOffsets[instanceIndex]);
        });
        const auto skinningEnd = std::chrono::steady_clock::now();
        viewer->BeginFrame();
        for (const auto& instance : instances) {
            instance->Upload();
            instance->Draw();
        }
        const auto uploadDrawEnd = std::chrono::steady_clock::now();
        if (!viewer->EndFrame())
            return false;
        const auto frameEnd = std::chrono::steady_clock::now();
        if (timing) {
            const auto Milliseconds = [](const auto start, const auto end) {
                return std::chrono::duration<double, std::milli>(end - start).count();
            };
            timing->animationMilliseconds = Milliseconds(animationStart, animationEnd);
            for (const auto& [initializeMilliseconds
                , animationEvaluateMilliseconds
                , morphMilliseconds
                , beforePhysicsPoseMilliseconds
                , physicsMilliseconds
                , afterPhysicsPoseMilliseconds
                , transformMilliseconds] : modelUpdateTimings) {
                timing->initializeCpuMilliseconds += initializeMilliseconds;
                timing->animationEvaluateCpuMilliseconds += animationEvaluateMilliseconds;
                timing->morphCpuMilliseconds += morphMilliseconds;
                timing->beforePhysicsPoseCpuMilliseconds += beforePhysicsPoseMilliseconds;
                timing->physicsCpuMilliseconds += physicsMilliseconds;
                timing->afterPhysicsPoseCpuMilliseconds += afterPhysicsPoseMilliseconds;
                timing->transformCpuMilliseconds += transformMilliseconds;
            }
            timing->skinningMilliseconds = Milliseconds(animationEnd, skinningEnd);
            timing->uploadDrawMilliseconds = Milliseconds(skinningEnd, uploadDrawEnd);
            timing->presentMilliseconds = Milliseconds(uploadDrawEnd, frameEnd);
            timing->totalMilliseconds = Milliseconds(frameStart, frameEnd);
        }
        TickFps();
        return true;
    }

    int Program::RunBenchmark(const std::size_t warmupFrames, const std::size_t benchmarkFrames) {
        for (std::size_t frame = 0; frame < warmupFrames; frame++) {
            if (!RunFrame())
                return 1;
        }
        FrameTiming total;
        FrameTiming maximum;
        for (std::size_t frame = 0; frame < benchmarkFrames; frame++) {
            FrameTiming timing;
            if (!RunFrame(&timing))
                return 1;
            total.animationMilliseconds += timing.animationMilliseconds;
            total.initializeCpuMilliseconds += timing.initializeCpuMilliseconds;
            total.animationEvaluateCpuMilliseconds += timing.animationEvaluateCpuMilliseconds;
            total.morphCpuMilliseconds += timing.morphCpuMilliseconds;
            total.beforePhysicsPoseCpuMilliseconds += timing.beforePhysicsPoseCpuMilliseconds;
            total.physicsCpuMilliseconds += timing.physicsCpuMilliseconds;
            total.afterPhysicsPoseCpuMilliseconds += timing.afterPhysicsPoseCpuMilliseconds;
            total.transformCpuMilliseconds += timing.transformCpuMilliseconds;
            total.skinningMilliseconds += timing.skinningMilliseconds;
            total.uploadDrawMilliseconds += timing.uploadDrawMilliseconds;
            total.presentMilliseconds += timing.presentMilliseconds;
            total.totalMilliseconds += timing.totalMilliseconds;
            maximum.animationMilliseconds = std::max(maximum.animationMilliseconds, timing.animationMilliseconds);
            maximum.initializeCpuMilliseconds = std::max(maximum.initializeCpuMilliseconds, timing.initializeCpuMilliseconds);
            maximum.animationEvaluateCpuMilliseconds = std::max(maximum.animationEvaluateCpuMilliseconds, timing.animationEvaluateCpuMilliseconds);
            maximum.morphCpuMilliseconds = std::max(maximum.morphCpuMilliseconds, timing.morphCpuMilliseconds);
            maximum.beforePhysicsPoseCpuMilliseconds = std::max(maximum.beforePhysicsPoseCpuMilliseconds, timing.beforePhysicsPoseCpuMilliseconds);
            maximum.physicsCpuMilliseconds = std::max(maximum.physicsCpuMilliseconds, timing.physicsCpuMilliseconds);
            maximum.afterPhysicsPoseCpuMilliseconds = std::max(maximum.afterPhysicsPoseCpuMilliseconds, timing.afterPhysicsPoseCpuMilliseconds);
            maximum.transformCpuMilliseconds = std::max(maximum.transformCpuMilliseconds, timing.transformCpuMilliseconds);
            maximum.skinningMilliseconds = std::max(maximum.skinningMilliseconds, timing.skinningMilliseconds);
            maximum.uploadDrawMilliseconds = std::max(maximum.uploadDrawMilliseconds, timing.uploadDrawMilliseconds);
            maximum.presentMilliseconds = std::max(maximum.presentMilliseconds, timing.presentMilliseconds);
            maximum.totalMilliseconds = std::max(maximum.totalMilliseconds, timing.totalMilliseconds);
        }
        const double frameCount = benchmarkFrames;
        const auto PrintMetric = [frameCount](const char* name, const double sum, const double max) {
            std::cout << name << "_avg_ms=" << sum / frameCount
                << ' ' << name << "_max_ms=" << max << '\n';
        };
        std::cout << std::fixed << std::setprecision(3);
        const auto physicsModelCount = std::ranges::count_if(instances, [](const auto& instance) {
            return instance->model->physicsData.physics != nullptr;
        });
        std::cout << "benchmark_renderer=" << ResolveRendererName(currentRendererType) << '\n';
        std::cout << "benchmark_models=" << instances.size() << '\n';
        std::cout << "benchmark_physics_models=" << physicsModelCount << '\n';
        std::cout << "benchmark_frames=" << benchmarkFrames << '\n';
        std::cout << "benchmark_warmup_frames=" << warmupFrames << '\n';
        PrintMetric("animation", total.animationMilliseconds, maximum.animationMilliseconds);
        PrintMetric("animation_initialize_cpu", total.initializeCpuMilliseconds, maximum.initializeCpuMilliseconds);
        PrintMetric("animation_evaluate_cpu", total.animationEvaluateCpuMilliseconds, maximum.animationEvaluateCpuMilliseconds);
        PrintMetric("morph_cpu", total.morphCpuMilliseconds, maximum.morphCpuMilliseconds);
        PrintMetric("pose_before_physics_cpu", total.beforePhysicsPoseCpuMilliseconds, maximum.beforePhysicsPoseCpuMilliseconds);
        PrintMetric("physics_cpu", total.physicsCpuMilliseconds, maximum.physicsCpuMilliseconds);
        PrintMetric("pose_after_physics_cpu", total.afterPhysicsPoseCpuMilliseconds, maximum.afterPhysicsPoseCpuMilliseconds);
        PrintMetric("transform_cpu", total.transformCpuMilliseconds, maximum.transformCpuMilliseconds);
        PrintMetric("skinning", total.skinningMilliseconds, maximum.skinningMilliseconds);
        PrintMetric("upload_draw", total.uploadDrawMilliseconds, maximum.uploadDrawMilliseconds);
        PrintMetric("present", total.presentMilliseconds, maximum.presentMilliseconds);
        PrintMetric("frame", total.totalMilliseconds, maximum.totalMilliseconds);
        std::cout << "benchmark_fps=" << 1000.0 / (total.totalMilliseconds / frameCount) << '\n';
        return 0;
    }

    int Program::Run(const int argumentCount, wchar_t* arguments[]) {
        if (argumentCount == 2 && std::wstring(arguments[1]) == L"--help") {
            PrintUsage();
            return 0;
        }
        ProgramOptions options;
        if (!ParseArguments(argumentCount, arguments, options)) {
            PrintUsage();
            return 1;
        }
        Language::Initialize();
        CreateViewer(options.rendererType);
        SceneConfig cfg;
        if (!options.scenePath.empty() && !cfg.Load(options.scenePath)) {
            std::cerr << "Failed to load scene config.\n";
            return 1;
        }
        benchmarkMode = options.benchmarkFrames > 0;
        cameraManager.Reset();
        inputManager.Reset();
        panelManager.Reset();
        panelManager.ApplySceneConfig(cfg);
        panelManager.SetRendererType(options.rendererType);
        if (!InitializeViewer()) {
            std::cerr << "Failed to run.\n";
            return 1;
        }
        DiscoverShaderPackages();
        panelManager.BindSound(music);
        panelManager.OpenGuiWindows();
        panelManager.SetModalFrameCallback([this] {
            return RunFrame(nullptr, false);
        });
        panelManager.UpdateFrameLimits(CalculatePlaybackLastFrame(), CalculateMotionLastFrame());
        fpsTime = std::chrono::steady_clock::now();
        saveTime = std::chrono::steady_clock::now();
        fpsFrame = 0;
        const auto loadStart = std::chrono::steady_clock::now();
        if (!LoadScene(cfg)) {
            Shutdown();
            return 1;
        }
        const auto loadEnd = std::chrono::steady_clock::now();
        if (benchmarkMode) {
            std::cout << std::fixed << std::setprecision(3)
                << "scene_load_ms="
                << std::chrono::duration<double, std::milli>(loadEnd - loadStart).count()
                << '\n';
        }
        cameraManager.UpdateCamera(*viewer);
        if (benchmarkMode) {
            const int result = RunBenchmark(options.warmupFrames, options.benchmarkFrames);
            Shutdown();
            return result;
        }
        while (!panelManager.IsCloseRequested() && !glfwWindowShouldClose(viewer->window)) {
            if (!RunFrame()) {
                Shutdown();
                return 1;
            }
        }
        Shutdown();
        return 0;
    }
}
