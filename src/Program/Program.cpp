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
#include "Viewer/Instance/Instance.h"
#include "Program/Shader/InternalShaderCatalog.h"
#include "Viewer/Viewer/OpenGlViewer.h"
#include "Viewer/Viewer/VulkanViewer.h"
#include "Viewer/Viewer/Dx11Viewer.h"
#include "Viewer/Viewer/Dx12Viewer.h"
#include "Core/Text/TextEncoding.h"
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
#include <utility>

namespace Chrivent {
    void Program::PrintUsage() {
        std::wcout
            << L"PmxMod [--scene <file.pmscene>] [--renderer <opengl|dx11|dx12|vulkan>]\n"
            << L"       [--benchmark <frames>] [--warmup <frames>]\n";
    }

    void Program::PrintGraphicsCapabilities(const GraphicsCapabilities& capabilities) {
        std::cout << "graphics_api=" << capabilities.apiName << '\n';
        std::cout << "graphics_api_version=" << capabilities.apiVersion << '\n';
        std::cout << "graphics_shader_version=" << capabilities.shaderVersion << '\n';
        std::cout << "graphics_gpu=" << capabilities.gpuName << '\n';
        std::cout << "graphics_gpu_type=" << capabilities.gpuType << '\n';
        std::cout << "graphics_max_samples=" << capabilities.maxSampleCount << '\n';
        std::cout << "graphics_active_samples=" << capabilities.activeSampleCount << '\n';
        std::cout << "graphics_uniform_alignment=" << capabilities.uniformBufferAlignment << '\n';
        std::cout << "graphics_max_texture_bindings=" << capabilities.maxTextureBindings << '\n';
        std::cout << "graphics_timeline_sync=" << capabilities.supportsTimelineSynchronization << '\n';
        std::cout << "graphics_dynamic_rendering=" << capabilities.supportsDynamicRendering << '\n';
        std::cout << "graphics_enhanced_barriers=" << capabilities.supportsEnhancedBarriers << '\n';
    }

    void Program::PrintGraphicsError(const GraphicsError& error) {
        std::cerr << error.Format() << '\n';
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
            case WM_EXITSIZEMOVE:
                program->RequestPhysicsReset();
                break;
            case WM_NCDESTROY:
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
                viewer = std::make_unique<OpenGlViewer>();
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
		auto shaderContractResult = InternalShaderCatalog::Load(
			resourceDirectories.GetInternalShaderDirectory(),
			viewer->IsNdcYInvertedForTextureCoordinates());
		if (!shaderContractResult) {
			std::cerr << shaderContractResult.error() << '\n';
			return false;
		}
        if (!glfwInit()) {
            std::cerr << "GLFW를 초기화하지 못했습니다.\n";
            return false;
        }
        glfwDefaultWindowHints();
        viewer->ConfigureWindowHints();
		GLFWwindow* viewerWindow = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
		if (!viewerWindow) {
            std::cerr << "렌더러 윈도우를 만들지 못했습니다.\n";
            viewer.reset();
            glfwTerminate();
            return false;
        }
		InstallViewerWindowSubclass(viewerWindow);
		inputManager.AttachWindow(viewerWindow);
		PositionViewerOnRightMonitor(viewerWindow);
		glfwMaximizeWindow(viewerWindow);
        glfwPollEvents();
		int framebufferWidth = 0;
		int framebufferHeight = 0;
		glfwGetFramebufferSize(viewerWindow, &framebufferWidth, &framebufferHeight);
		if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            std::cerr << "framebuffer 크기가 올바르지 않습니다.\n";
            RemoveViewerWindowSubclass();
            viewer.reset();
            glfwTerminate();
            return false;
        }
		const auto setupResult = viewer->Setup(viewerWindow, framebufferWidth, framebufferHeight,
			*shaderContractResult);
		if (!setupResult) {
            PrintGraphicsError(setupResult.error());
            RemoveViewerWindowSubclass();
            viewer.reset();
            glfwTerminate();
            return false;
        }
        PrintGraphicsCapabilities(viewer->GetGraphicsCapabilities());
        glfwPollEvents();
		glfwGetFramebufferSize(viewerWindow, &framebufferWidth, &framebufferHeight);
		if (framebufferWidth != viewer->GetScreenWidth() || framebufferHeight != viewer->GetScreenHeight()) {
			const auto resizeResult = viewer->Resize(framebufferWidth, framebufferHeight);
			if (!resizeResult) {
				PrintGraphicsError(resizeResult.error());
                RemoveViewerWindowSubclass();
                viewer.reset();
                glfwTerminate();
                return false;
            }
        }
		fpsOverlay.Create(viewerWindow);
        return true;
    }

	void Program::InstallViewerWindowSubclass(GLFWwindow* window) {
		if (!window)
            return;
		viewerNativeWindow = glfwGetWin32Window(window);
        if (viewerNativeWindow)
            SetWindowSubclass(viewerNativeWindow, ViewerWindowProc, kViewerWindowSubclassId, reinterpret_cast<DWORD_PTR>(this));
    }

    void Program::RemoveViewerWindowSubclass() {
        if (!viewerNativeWindow)
            return;
        RemoveWindowSubclass(viewerNativeWindow, ViewerWindowProc, kViewerWindowSubclassId);
        viewerNativeWindow = nullptr;
    }

    void Program::RequestPhysicsReset() {
        physicsResetRequested = true;
    }

    bool Program::RunMenuFrame() {
        if (menuFrameActive)
            return true;
        menuFrameActive = true;
        const bool result = RunFrame(nullptr, false);
        menuFrameActive = false;
        return result;
    }

	void Program::PositionViewerOnRightMonitor(GLFWwindow* window) {
		if (!window)
			return;
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
		glfwGetWindowSize(window, &windowWidth, &windowHeight);
        const int x = rightX + std::max(0, (rightWidth - windowWidth) / 2);
        const int y = rightY + std::max(0, (rightHeight - windowHeight) / 2);
		glfwSetWindowPos(window, x, y);
    }

    bool Program::ChangeRenderer(const RendererType rendererType) {
		if (rendererType == currentRendererType)
			return true;
		const int playbackFrame = viewer
			? cameraManager.GetAnimationFrame() + 0.5f : 0;
		GLFWwindow* previousWindow = viewer ? viewer->GetWindow() : nullptr;
        if (viewer) {
			const auto waitResult = viewer->WaitIdle();
			if (!waitResult) {
				PrintGraphicsError(waitResult.error());
				return false;
			}
		}
        fpsOverlay.Reset();
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
        ApplyShaderEffects();
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
		GLFWwindow* window = viewer ? viewer->GetWindow() : nullptr;
        if (viewer) {
			const auto waitResult = viewer->WaitIdle();
			if (!waitResult)
				PrintGraphicsError(waitResult.error());
		}
        fpsOverlay.Reset();
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
        shaderEffectEntries.clear();
        if (!viewer)
            return;
		auto [packages, errors] = ShaderPackageLoader::Discover(resourceDirectories.GetShaderPackagesDirectory());
        shaderPackages = std::move(packages);
        for (const auto& error : errors)
            std::cerr << "셰이더 패키지를 불러오지 못했습니다: " << error.Format() << '\n';
        BuildShaderEffectEntries();
        const size_t effectCount = shaderEffectEntries.size();
        std::cout << "shader_packages=" << shaderPackages.size() << '\n';
        std::cout << "effects=" << effectCount << '\n';
        selectedShaderEffectIndex = effectCount == 0 ? 0 : std::min(selectedShaderEffectIndex, effectCount - 1);
        if (shaderEffectEnabled.size() != effectCount) {
            std::vector newEnabled(effectCount, false);
            const size_t copyCount = std::min(shaderEffectEnabled.size(), newEnabled.size());
            for (size_t index = 0; index < copyCount; index++)
                newEnabled[index] = shaderEffectEnabled[index];
            shaderEffectEnabled = std::move(newEnabled);
        }
        UpdateShaderPanel();
        ApplyShaderEffects();
    }

    void Program::BuildShaderEffectEntries() {
        shaderEffectEntries.clear();
        for (size_t packageIndex = 0; packageIndex < shaderPackages.size(); packageIndex++) {
            const auto& package = shaderPackages[packageIndex];
			for (size_t effectIndex = 0; effectIndex < package.effects.size(); effectIndex++)
				shaderEffectEntries.push_back({packageIndex, effectIndex});
        }
    }

    void Program::UpdateShaderPanel() {
        std::vector<std::wstring> shaderNames;
        shaderNames.reserve(shaderEffectEntries.size());
        for (const auto& [packageIndex, effectIndex] : shaderEffectEntries) {
            const auto& package = shaderPackages[packageIndex];
            const auto& effect = package.effects[effectIndex];
            shaderNames.emplace_back(
                TextEncoding::Utf8ToWide(package.name) + L" / " + TextEncoding::Utf8ToWide(effect.name));
        }
        panelManager.ApplyShaderNames(shaderNames, selectedShaderEffectIndex, shaderEffectEnabled);
		const SceneRenderState& scene = viewer->GetSceneRenderState();
		panelManager.ApplyBuiltInShaderStates(scene.modelEnabled, scene.edgeEnabled, scene.groundShadowEnabled);
    }

    void Program::ApplyShaderEffects() const {
        if (!viewer)
            return;
        std::vector<const EffectRuntimeDefinition*> postProcessEffects;
        for (size_t index = 0; index < shaderEffectEntries.size(); index++) {
            const auto& [packageIndex, effectIndex] = shaderEffectEntries[index];
            const auto& effect = shaderPackages[packageIndex].effects[effectIndex];
            if (index < shaderEffectEnabled.size() && shaderEffectEnabled[index])
                postProcessEffects.push_back(&effect.runtime);
        }
        const auto loadResult = viewer->LoadPostProcessEffects(postProcessEffects);
        if (loadResult) {
            std::cout << "active_post_effects=" << postProcessEffects.size() << '\n';
            return;
        }
        PrintGraphicsError(loadResult.error());
    }

    bool Program::LoadScene(const SceneConfig& sceneConfig, const bool resetPlaybackRange) {
        music.Pause();
        std::vector<std::unique_ptr<Instance>> loadedInstances;
        if (!LoadInstances(sceneConfig, loadedInstances)) {
            std::cerr << "장면 인스턴스를 불러오지 못했습니다.\n";
            return false;
        }
        const auto waitResult = viewer->WaitIdle();
        if (!waitResult) {
            PrintGraphicsError(waitResult.error());
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
        saveTime = std::chrono::steady_clock::now();
        cameraManager.Stop(*viewer, music, saveTime);
        panelManager.UpdateFrameLimits(CalculatePlaybackLastFrame(), CalculateMotionLastFrame(), resetPlaybackRange);
        panelManager.ApplyCameraMotionPath(sceneConfig.cameraAnim);
        const int startFrame = panelManager.GetPlaybackFrameRange().start;
		if (startFrame > 0) {
			cameraManager.SeekFrame(*viewer, music, startFrame, saveTime);
			ResetPhysics(startFrame);
		}
		viewer->ResetPostProcessHistory();
		return true;
    }

    bool Program::LoadInstances(const SceneConfig& sceneConfig, std::vector<std::unique_ptr<Instance>>& loadedInstances) const {
        loadedInstances.clear();
        loadedInstances.reserve(sceneConfig.modelConfigs.size());
        for (const auto& [modelPath, animPaths, scale] : sceneConfig.modelConfigs) {
            const auto pmxModel = std::make_shared<Model>();
			const auto loadResult = ModelLoader::Load(
				*pmxModel, modelPath, resourceDirectories.GetDefaultToonTextureDirectory());
			if (!loadResult) {
                std::cerr << loadResult.error().message << '\n';
                return false;
            }
            ModelAnimator::InitializeAnimation(*pmxModel);
            AnimationBuilder animationBuilder(pmxModel);
            for (const auto& vmdPath : animPaths) {
                VmdParser vmd;
                const auto parseResult = vmd.ReadFile(vmdPath);
                if (!parseResult) {
                    std::cerr << "VMD 파일을 읽지 못했습니다: "
						<< BinaryReader::FormatParseError(parseResult.error()) << '\n';
                    return false;
                }
                animationBuilder.Build(vmd.GetData());
            }
            auto vmdAnim = animationBuilder.TakeAnimation();
            ModelAnimator::SyncPhysics(*pmxModel, *vmdAnim, 0.0f);
            auto instanceResult = viewer->CreateInstance(pmxModel, std::move(vmdAnim), scale);
            if (!instanceResult) {
				PrintGraphicsError(instanceResult.error());
                return false;
			}
            loadedInstances.emplace_back(std::move(*instanceResult));
        }
        return true;
    }

    int Program::CalculateMotionLastFrame() const {
        int lastFrame = cameraManager.CalculateLastFrame();
        for (const auto& instance : instances) {
            if (instance && instance->GetAnimation()) {
                const uint32_t animationLastFrame = instance->GetAnimation()->CalculateLastFrame();
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
            if (!instance || !instance->GetAnimation())
                continue;
            Model& model = instance->GetModel();
            ModelAnimator::BeginAnimation(model);
            instance->GetAnimation()->Evaluate(frame);
            ModelAnimator::UpdateMorphAnimation(model);
            ModelPose::UpdateNodeAnimation(model, false);
            ModelPose::UpdateNodeAnimation(model, true);
            ModelPose::ResetPhysics(model);
            ModelAnimator::SyncPhysics(model, *instance->GetAnimation(), frame);
        }
        viewer->ResetPostProcessHistory();
    }

    void Program::UpdateMotionPanel(const size_t modelIndex) {
        if (modelIndex >= instances.size() || !instances[modelIndex])
            return;
        const auto& instance = *instances[modelIndex];
        const auto& model = instance.GetModel();
        const auto* animation = instance.GetAnimation();
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
        if (animation) {
            for (const auto& [node, keys] : animation->GetNodeTracks()) {
                auto& timelineKeys = nodeKeys[node];
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
            for (const auto& [ikSolver, keys] : animation->GetIkTracks()) {
                auto& timelineKeys = ikKeys[ikSolver];
                timelineKeys.reserve(keys.size());
                for (const auto& [frame, ikEnable] : keys)
                    timelineKeys.push_back({.frame = ToTimelineFrame(frame)});
            }
            for (const auto& [morph, keys] : animation->GetMorphTracks()) {
                auto& timelineKeys = morphKeys[morph];
                timelineKeys.reserve(keys.size());
                for (const auto& [frame, morphWeight] : keys)
                    timelineKeys.push_back({.frame = ToTimelineFrame(frame)});
            }
        }
        std::vector<MotionTimelineGroup> groups;
        groups.reserve(model.skeletonData.displayFrames.size() + 1);
		const auto& cameraKeys = cameraManager.GetAnimationKeys();
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
                .name = TextEncoding::Utf8ToWide(name)
            };
            group.rows.reserve(boneIndices.size() + morphIndices.size());
            for (const uint32_t boneIndex : boneIndices) {
                if (boneIndex >= model.skeletonData.GetNodes().size())
                    continue;
                const auto& node = model.skeletonData.GetNodes()[boneIndex];
                if (!node)
                    continue;
                auto keys = nodeKeys[node.get()];
                if (const auto ikSolver = node->ikSolver.lock()) {
                    const auto& solverKeys = ikKeys[ikSolver.get()];
                    keys.insert(keys.end(), solverKeys.begin(), solverKeys.end());
                }
                NormalizeKeys(keys);
                group.rows.push_back({
                    .name = TextEncoding::Utf8ToWide(node->name),
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
                if (morphIndex >= model.morphData.GetMorphs().size())
                    continue;
                const auto& morph = model.morphData.GetMorphs()[morphIndex];
                if (!morph)
                    continue;
                auto keys = morphKeys[morph.get()];
                NormalizeKeys(keys);
                group.rows.push_back({
                    .name = TextEncoding::Utf8ToWide(morph->name),
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
            for (const auto& node : model.skeletonData.GetNodes()) {
                if (!node)
                    continue;
                auto keys = nodeKeys[node.get()];
                if (const auto ikSolver = node->ikSolver.lock()) {
                    const auto& solverKeys = ikKeys[ikSolver.get()];
                    keys.insert(keys.end(), solverKeys.begin(), solverKeys.end());
                }
                NormalizeKeys(keys);
                boneGroup.rows.push_back({
                    .name = TextEncoding::Utf8ToWide(node->name),
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
            for (const auto& morph : model.morphData.GetMorphs()) {
                if (!morph)
                    continue;
                auto keys = morphKeys[morph.get()];
                NormalizeKeys(keys);
                morphGroup.rows.push_back({
                    .name = TextEncoding::Utf8ToWide(morph->name),
                    .keys = std::move(keys)
                });
            }
            morphGroup.keyFrames = CollectFrames(morphGroup.rows);
            if (!morphGroup.rows.empty())
                groups.emplace_back(std::move(morphGroup));
        }
        std::wstring modelName = TextEncoding::Utf8ToWide(model.infoData.modelName);
        if (modelName.empty() && modelIndex < panelManager.GetSceneConfig().modelConfigs.size())
            modelName = panelManager.GetSceneConfig().modelConfigs[modelIndex].modelPath.filename().wstring();
        panelManager.ApplyMotionTimeline(std::move(modelName), std::move(groups));
    }

    void Program::UpdateCameraMotionPanel() {
		const auto& cameraKeys = cameraManager.GetAnimationKeys();
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

    void Program::UpdateShaderMotionPanel(const size_t shaderEffectIndex) {
        if (shaderEffectIndex < shaderEffectEntries.size()) {
            const auto& [packageIndex, effectIndex] = shaderEffectEntries[shaderEffectIndex];
            const auto& package = shaderPackages[packageIndex];
            const auto& effect = package.effects[effectIndex];
            const std::wstring name =
                TextEncoding::Utf8ToWide(package.name) + L" / " + TextEncoding::Utf8ToWide(effect.name);
            panelManager.ApplyMotionTimeline(name, {});
            return;
        }
        panelManager.ApplyMotionTimeline(Language::Text("panel.camera"), {});
    }

    void Program::ClearInstances() {
        instances.clear();
    }

    GraphicsError::Result<Program::FramebufferUpdateState> Program::UpdateFramebufferSize() const {
        int newW = 0;
        int newH = 0;
        glfwGetFramebufferSize(viewer->GetWindow(), &newW, &newH);
        if (newW <= 0 || newH <= 0)
            return FramebufferUpdateState::Skipped;
        if (newW == viewer->GetScreenWidth() && newH == viewer->GetScreenHeight())
            return FramebufferUpdateState::Ready;
		const auto resizeResult = viewer->Resize(newW, newH);
		if (!resizeResult)
			return std::unexpected(resizeResult.error());
        return FramebufferUpdateState::Ready;
    }

    void Program::TickFps() {
        fpsFrame++;
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsTime).count();
        if (sec > 1.0) {
            fpsOverlay.Update(fpsFrame / sec);
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
            ApplyShaderEffects();
            panelManager.ApplyMotionMode(MotionTimelineMode::Camera);
            UpdateShaderMotionPanel(selectedShaderEffectIndex);
        }
        BuiltInShaderToggle builtInShader;
        bool builtInShaderEnabled = false;
		SceneRenderState& scene = viewer->GetSceneRenderState();
        if (panelManager.ConsumeBuiltInShaderToggle(builtInShader, builtInShaderEnabled)) {
            switch (builtInShader) {
                case BuiltInShaderToggle::Model:
					scene.modelEnabled = builtInShaderEnabled;
                    break;
                case BuiltInShaderToggle::Edge:
					scene.edgeEnabled = builtInShaderEnabled;
                    break;
                case BuiltInShaderToggle::GroundShadow:
					scene.groundShadowEnabled = builtInShaderEnabled;
                    break;
            }
        }
        int seekFrame = 0;
        bool seekFinished = false;
        if (panelManager.ConsumeSeekFrame(seekFrame, seekFinished)) {
			cameraManager.SetPhysicsSkipped(!seekFinished);
            cameraManager.SeekFrame(*viewer, music, seekFrame, saveTime);
            if (seekFinished) {
                ResetPhysics(seekFrame);
				cameraManager.SetPhysicsSkipped(false);
            }
        }
        switch (panelManager.ConsumePlaybackCommand()) {
            case PlaybackCommand::Play:
				cameraManager.SetPhysicsSkipped(false);
                if (const auto [start, end] = panelManager.GetPlaybackFrameRange();
					cameraManager.GetAnimationFrame() < start ||
					cameraManager.GetAnimationFrame() >= end) {
                    cameraManager.SeekFrame(*viewer, music, start, saveTime);
                    ResetPhysics(start);
                }
                cameraManager.Play(music);
                break;
            case PlaybackCommand::Pause:
                cameraManager.Pause(music);
                break;
            case PlaybackCommand::Stop:
				cameraManager.SetPhysicsSkipped(false);
                cameraManager.Stop(*viewer, music, saveTime);
                cameraManager.SeekFrame(*viewer, music, 0, saveTime);
                ResetPhysics(0);
                break;
            case PlaybackCommand::None:
                break;
        }
        cameraManager.ApplyMotionCameraState(*viewer, panelManager.IsCameraMode());
        inputManager.Update(*viewer);
        cameraManager.HandleInput(inputManager, music);
        const auto framebufferResult = UpdateFramebufferSize();
		if (!framebufferResult) {
			PrintGraphicsError(framebufferResult.error());
			return false;
		}
        switch (*framebufferResult) {
            case FramebufferUpdateState::Skipped:
                TickFps();
                return true;
            case FramebufferUpdateState::Ready:
                break;
        }
        if (benchmarkMode) {
			cameraManager.StepFixedTime(1.0f / 30.0f);
        } else
            cameraManager.StepTime(music, saveTime);
        const int endFrame = panelManager.GetPlaybackFrameRange().end;
		const float playbackFrame = cameraManager.GetAnimationFrame();
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
			cameraManager.SetPhysicsSkipped(false);
        }
        bool shouldResetPhysics = physicsResetRequested;
        physicsResetRequested = false;
        if (panelManager.ConsumePhysicsDirty())
            shouldResetPhysics = true;
        if (shouldResetPhysics) {
			ResetPhysics(cameraManager.GetAnimationFrame() + 0.5f);
			cameraManager.SetPhysicsSkipped(false);
        }
        panelManager.ApplyPlaybackState(cameraManager.IsPlaying());
		panelManager.SetPlaybackFrame(cameraManager.GetAnimationFrame() + 0.5f);
        cameraManager.UpdateCamera(*viewer);
        fpsOverlay.SetVisible(panelManager.IsFpsVisible());
        const auto animationStart = std::chrono::steady_clock::now();
        if (timing) {
            modelUpdateTimings.clear();
            modelUpdateTimings.resize(instances.size());
        }
        const bool physicsEnabled = panelManager.IsPhysicsEnabled();
		const InstanceUpdateState instanceUpdateState{
			.animationFrame = cameraManager.GetAnimationFrame(),
			.elapsed = cameraManager.GetElapsed(),
			.velocityRequired = viewer->RequiresPostProcessVelocity(),
			.physicsEnabled = physicsEnabled && !cameraManager.IsPhysicsSkipped()
		};
        taskExecutor.Run(instances.size(), [&](const std::size_t index) {
			instances[index]->PrepareUpdate(instanceUpdateState, timing ? &modelUpdateTimings[index] : nullptr);
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
        const auto frameBeginResult = viewer->BeginFrame();
        if (!frameBeginResult) {
			PrintGraphicsError(frameBeginResult.error());
            return false;
		}
        if (*frameBeginResult == FrameBeginState::Skipped) {
            TickFps();
            return true;
        }
        const SceneDrawState drawState = viewer->ResolveSceneDrawState();
        for (const auto& instance : instances) {
			const auto uploadResult = instance->Upload();
            if (!uploadResult) {
				PrintGraphicsError(uploadResult.error());
                return false;
			}
            instance->BeginDraw(drawState);
        }
        for (const auto& instance : instances) {
			const auto drawResult = instance->DrawModelPass();
            if (!drawResult) {
				PrintGraphicsError(drawResult.error());
                return false;
			}
        }
        for (const auto& instance : instances) {
			const auto drawResult = instance->DrawEdgePass();
            if (!drawResult) {
				PrintGraphicsError(drawResult.error());
                return false;
			}
        }
        for (const auto& instance : instances) {
			const auto drawResult = instance->DrawGroundShadowPass();
            if (!drawResult) {
				PrintGraphicsError(drawResult.error());
                return false;
			}
        }
        const auto sceneInputResult = viewer->BeginPostProcessSceneInputPass();
        if (!sceneInputResult) {
			PrintGraphicsError(sceneInputResult.error());
            return false;
		}
        if (*sceneInputResult == PostProcessSceneInputState::Ready) {
            for (const auto& instance : instances) {
				const auto drawResult = instance->DrawPostProcessSceneInputs();
                if (!drawResult) {
					PrintGraphicsError(drawResult.error());
                    return false;
				}
            }
            const auto sceneInputEndResult = viewer->EndPostProcessSceneInputPass();
            if (!sceneInputEndResult) {
				PrintGraphicsError(sceneInputEndResult.error());
                return false;
			}
        }
        const auto uploadDrawEnd = std::chrono::steady_clock::now();
        const auto frameEndResult = viewer->EndFrame();
        if (!frameEndResult) {
			PrintGraphicsError(frameEndResult.error());
            return false;
		}
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
            return instance->GetModel().HasPhysics();
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

    Program::Program() = default;
    Program::~Program() = default;

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
		if (!resourceDirectories.Initialize()) {
			std::cerr << "리소스 디렉터리 경로를 확인하지 못했습니다.\n";
			return 1;
		}
        CreateViewer(options.rendererType);
        SceneConfig cfg;
        if (!options.scenePath.empty() && !cfg.Load(options.scenePath)) {
            std::cerr << "장면 설정을 불러오지 못했습니다.\n";
            return 1;
        }
        benchmarkMode = options.benchmarkFrames > 0;
        cameraManager.Reset();
        inputManager.Reset();
        panelManager.Reset();
        panelManager.ApplySceneConfig(cfg);
        panelManager.SetRendererType(options.rendererType);
        if (!InitializeViewer()) {
            std::cerr << "프로그램 실행 중 오류가 발생했습니다.\n";
            return 1;
        }
        DiscoverShaderPackages();
        panelManager.BindSound(music);
        panelManager.OpenGuiWindows();
        panelManager.SetInteractionFinishedCallback([this] {
            RequestPhysicsReset();
        });
        panelManager.SetMenuFrameCallback([this] {
            return RunMenuFrame();
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
		while (!panelManager.IsCloseRequested() && !glfwWindowShouldClose(viewer->GetWindow())) {
            if (!RunFrame()) {
                Shutdown();
                return 1;
            }
        }
        Shutdown();
        return 0;
    }
}
