#include "Program.h"

#include "../Animation/Camera/CameraAnimation.h"
#include "../Animation/Model/Animation.h"
#include "../Animation/Model/AnimationBuilder.h"
#include "../Model/ModelLoader.h"
#include "../Model/ModelAnimator.h"
#include "../Parser/VmdParser.h"
#include "../Viewer/Glsl/Glfw/GlfwViewer.h"
#include "../Viewer/Glsl/Vulkan/VulkanViewer.h"
#include "../Viewer/Hlsl/Dx11/Dx11Viewer.h"
#include "../Viewer/Hlsl/Dx12/Dx12Viewer.h"
#include "../Util.h"
#include "Language.h"

#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace Chrivent {
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
        viewer->ConfigureGlfwHints();
        viewer->GetInfo().window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
        if (!viewer->GetInfo().window) {
            std::cerr << "Failed to create viewer window.\n";
            viewer.reset();
            glfwTerminate();
            return false;
        }
        PositionViewerOnRightMonitor();
        glfwMaximizeWindow(viewer->GetInfo().window);
        glfwPollEvents();
        glfwGetFramebufferSize(viewer->GetInfo().window, &viewer->GetInfo().screenWidth, &viewer->GetInfo().screenHeight);
        if (viewer->GetInfo().screenWidth <= 0 || viewer->GetInfo().screenHeight <= 0) {
            std::cerr << "Invalid framebuffer size.\n";
            viewer.reset();
            glfwTerminate();
            return false;
        }
        if (!viewer->Setup()) {
            std::cerr << "Failed to set up renderer.\n";
            viewer.reset();
            glfwTerminate();
            return false;
        }
        if (!viewer->Resize()) {
            viewer.reset();
            glfwTerminate();
            return false;
        }
        glfwPollEvents();
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(viewer->GetInfo().window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth != viewer->GetInfo().screenWidth ||
            framebufferHeight != viewer->GetInfo().screenHeight) {
            viewer->GetInfo().screenWidth = framebufferWidth;
            viewer->GetInfo().screenHeight = framebufferHeight;
            if (!viewer->Resize()) {
                viewer.reset();
                glfwTerminate();
                return false;
            }
        }
        viewer->CreateFpsOverlay();
        return true;
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
            leftmostX = (std::min)(leftmostX, x);
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
        glfwGetWindowSize(viewer->GetInfo().window, &windowWidth, &windowHeight);
        const int x = rightX + (std::max)(0, (rightWidth - windowWidth) / 2);
        const int y = rightY + (std::max)(0, (rightHeight - windowHeight) / 2);
        glfwSetWindowPos(viewer->GetInfo().window, x, y);
    }

    bool Program::ChangeRenderer(const RendererType rendererType) {
        if (rendererType == currentRendererType)
            return true;
        ClearInstances();
        if (viewer && viewer->GetInfo().window)
            glfwDestroyWindow(viewer->GetInfo().window);
        viewer.reset();
        CreateViewer(rendererType);
        if (!InitializeViewer())
            return false;
        saveTime = std::chrono::steady_clock::now();
        if (!LoadScene(panelManager.GetSceneConfig(), false))
            return false;
        panelManager.BindSound(music);
        cameraManager.UpdateCamera(viewer->GetInfo());
        return true;
    }

    void Program::Shutdown() {
        ClearInstances();
        music.Stop();
        panelManager.DestroyGui();
        if (viewer && viewer->GetInfo().window)
            glfwDestroyWindow(viewer->GetInfo().window);
        viewer.reset();
        glfwTerminate();
    }

    bool Program::LoadScene(const SceneConfig& sceneConfig, const bool resetPlaybackRange) {
        std::vector<std::unique_ptr<Instance>> loadedInstances;
        if (!LoadInstances(sceneConfig, loadedInstances)) {
            std::cerr << "Failed to load scene instances.\n";
            return false;
        }
        ClearInstances();
        instances = std::move(loadedInstances);
        music.Stop();
        if (!sceneConfig.musicPath.empty())
            music.Init(sceneConfig.musicPath, false);
        cameraManager.LoadCameraAnim(sceneConfig.cameraAnim);
        viewer->GetInfo().elapsed = 0.0f;
        viewer->GetInfo().animTime = 0.0f;
        viewer->GetInfo().skipPhysics = false;
        saveTime = std::chrono::steady_clock::now();
        cameraManager.Stop(viewer->GetInfo(), music, saveTime);
        panelManager.SetFrameLimits(
            CalculatePlaybackLastFrame(),
            CalculateMotionLastFrame(),
            resetPlaybackRange);
        const int startFrame = panelManager.GetPlaybackFrameRange().start;
        if (startFrame > 0) {
            cameraManager.SeekFrame(viewer->GetInfo(), music, startFrame, saveTime);
            SyncSeekedPhysics(startFrame);
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
            if (!loader.Load(modelPath, viewer->GetInfo().pmxDir)) {
                std::cerr << "Failed to load pmx file.\n";
                return false;
            }
            instance->GetInfo().model = pmxModel;
            const ModelAnimator animator(*instance->GetInfo().model);
            animator.InitializeAnimation();
            AnimationBuilder animationBuilder(instance->GetInfo().model);
            for (const auto& vmdPath : animPaths) {
                VmdParser vmd;
                if (!vmd.ReadFile(vmdPath.c_str())) {
                    std::cerr << "Failed to read VMD file.\n";
                    return false;
                }
                animationBuilder.Build(vmd.GetData());
            }
            auto vmdAnim = std::make_unique<Animation>(animationBuilder.TakeInfo());
            animator.SyncPhysics(*vmdAnim, 0.0f);
            instance->GetInfo().anim = std::move(vmdAnim);
            instance->GetInfo().scale = scale;
            if (!instance->Setup(*viewer))
                return false;
            loadedInstances.emplace_back(std::move(instance));
        }
        return true;
    }

    int Program::CalculateMotionLastFrame() const {
        uint32_t lastFrame = cameraManager.GetLastFrame();
        for (const auto& instance : instances) {
            if (instance && instance->GetInfo().anim)
                lastFrame = (std::max)(lastFrame, instance->GetInfo().anim->GetLastFrame());
        }
        return lastFrame;
    }

    int Program::CalculatePlaybackLastFrame() const {
        uint32_t lastFrame = CalculateMotionLastFrame();
        if (music.HasSound())
            lastFrame = (std::max)(lastFrame, static_cast<uint32_t>(std::ceil(music.GetLengthSeconds() * 30.0)));
        return lastFrame;
    }

    void Program::SyncSeekedPhysics(const int frame) const {
        for (const auto& instance : instances) {
            if (!instance || !instance->GetInfo().model || !instance->GetInfo().anim)
                continue;
            const ModelAnimator animator(*instance->GetInfo().model);
            animator.BeginAnimation();
            animator.SyncPhysics(*instance->GetInfo().anim, frame);
        }
    }

    void Program::UpdateMotionPanel(const size_t modelIndex) {
        if (modelIndex >= instances.size() || !instances[modelIndex] || !instances[modelIndex]->GetInfo().model)
            return;
        const auto& instanceInfo = instances[modelIndex]->GetInfo();
        const auto& model = *instanceInfo.model;
        const auto MakeCurve = [](std::wstring name, const Bezier& bezier) {
            return MotionBezierCurve{
                .name = std::move(name),
                .controlPoints = bezier.GetControlPoints()
            };
        };
        const auto NormalizeKeys = [](std::vector<MotionTimelineKey>& keys) {
            std::ranges::sort(keys, {}, &MotionTimelineKey::frame);
            std::vector<MotionTimelineKey> normalized;
            normalized.reserve(keys.size());
            for (auto& key : keys) {
                if (!normalized.empty() && normalized.back().frame == key.frame) {
                    if (normalized.back().curves.empty() && !key.curves.empty())
                        normalized.back().curves = std::move(key.curves);
                    continue;
                }
                normalized.emplace_back(std::move(key));
            }
            keys = std::move(normalized);
        };
        const auto CollectFrames = [](const std::vector<MotionTimelineRow>& rows) {
            std::vector<uint32_t> frames;
            for (const auto& [name, keys] : rows) {
                for (const auto& key : keys)
                    frames.emplace_back(key.frame);
            }
            std::ranges::sort(frames);
            const auto uniqueFrames = std::ranges::unique(frames);
            frames.erase(uniqueFrames.begin(), uniqueFrames.end());
            return frames;
        };
        std::unordered_map<const Node*, std::vector<MotionTimelineKey>> nodeKeys;
        std::unordered_map<const IkSolver*, std::vector<MotionTimelineKey>> ikKeys;
        std::unordered_map<const Morph*, std::vector<MotionTimelineKey>> morphKeys;
        if (instanceInfo.anim) {
            const auto& [nodeTracks, ikTracks, morphTracks] = instanceInfo.anim->GetInfo();
            for (const auto& [node, keys] : nodeTracks) {
                auto& timelineKeys = nodeKeys[node.get()];
                timelineKeys.reserve(keys.size());
                for (const auto& key : keys) {
                    timelineKeys.push_back({
                        .frame = key.frame,
                        .curves = {
                            MakeCurve(Language::Text("interpolation.x"), key.txBezier),
                            MakeCurve(Language::Text("interpolation.y"), key.tyBezier),
                            MakeCurve(Language::Text("interpolation.z"), key.tzBezier),
                            MakeCurve(Language::Text("interpolation.rotation"), key.rotBezier)
                        }
                    });
                }
            }
            for (const auto& [ikSolver, keys] : ikTracks) {
                auto& timelineKeys = ikKeys[ikSolver.get()];
                timelineKeys.reserve(keys.size());
                for (const auto& [frame, ikEnable] : keys)
                    timelineKeys.push_back({.frame = frame});
            }
            for (const auto& [morph, keys] : morphTracks) {
                auto& timelineKeys = morphKeys[morph.get()];
                timelineKeys.reserve(keys.size());
                for (const auto& [frame, morphWeight] : keys)
                    timelineKeys.push_back({.frame = frame});
            }
        }
        std::vector<MotionTimelineGroup> groups;
        groups.reserve(model.skeletonData.displayFrames.size() + 1);
        const auto& cameraKeys = cameraManager.GetAnimationKeys();
        if (!cameraKeys.empty()) {
            MotionTimelineRow cameraRow{.name = Language::Text("motion.camera")};
            cameraRow.keys.reserve(cameraKeys.size());
            for (const auto& key : cameraKeys) {
                cameraRow.keys.push_back({
                    .frame = key.frame,
                    .curves = {
                        MakeCurve(Language::Text("interpolation.x"), key.ixBezier),
                        MakeCurve(Language::Text("interpolation.y"), key.iyBezier),
                        MakeCurve(Language::Text("interpolation.z"), key.izBezier),
                        MakeCurve(Language::Text("interpolation.rotation"), key.rotateBezier),
                        MakeCurve(Language::Text("interpolation.distance"), key.distanceBezier),
                        MakeCurve(Language::Text("interpolation.fov"), key.fovBezier)
                    }
                });
            }
            MotionTimelineGroup cameraGroup{
                .name = Language::Text("motion.camera"),
                .rows = {std::move(cameraRow)},
                .mode = MotionTimelineMode::Camera,
                .grouped = false
            };
            cameraGroup.keyFrames = CollectFrames(cameraGroup.rows);
            groups.emplace_back(std::move(cameraGroup));
        }
        for (const auto& [name
            , boneIndices
            , morphIndices] : model.skeletonData.displayFrames) {
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
                if (const auto ikSolver = node->GetInfo().ikSolver.lock()) {
                    const auto& solverKeys = ikKeys[ikSolver.get()];
                    keys.insert(keys.end(), solverKeys.begin(), solverKeys.end());
                }
                NormalizeKeys(keys);
                group.rows.push_back({
                    .name = Util::Utf8ToWString(node->GetInfo().name),
                    .keys = std::move(keys)
                });
            }
            for (const uint32_t morphIndex : morphIndices) {
                if (morphIndex >= model.morphData.morphs.size())
                    continue;
                const auto& morph = model.morphData.morphs[morphIndex];
                if (!morph)
                    continue;
                auto keys = morphKeys[morph.get()];
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
                if (const auto ikSolver = node->GetInfo().ikSolver.lock()) {
                    const auto& solverKeys = ikKeys[ikSolver.get()];
                    keys.insert(keys.end(), solverKeys.begin(), solverKeys.end());
                }
                NormalizeKeys(keys);
                boneGroup.rows.push_back({
                    .name = Util::Utf8ToWString(node->GetInfo().name),
                    .keys = std::move(keys)
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
        panelManager.SetMotionTimeline(std::move(modelName), std::move(groups));
    }

    void Program::ClearInstances() {
        for (const auto& instance : instances)
            instance->Clear();
        instances.clear();
    }

    bool Program::UpdateFramebufferSize() const {
        int newW = 0;
        int newH = 0;
        glfwGetFramebufferSize(viewer->GetInfo().window, &newW, &newH);
        if (newW == viewer->GetInfo().screenWidth && newH == viewer->GetInfo().screenHeight)
            return true;
        viewer->GetInfo().screenWidth = newW;
        viewer->GetInfo().screenHeight = newH;
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

    bool Program::RunFrame() {
        glfwPollEvents();
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
        size_t selectedModelIndex = 0;
        if (panelManager.ConsumeSelectedModelIndex(selectedModelIndex))
            UpdateMotionPanel(selectedModelIndex);
        PlaybackFrameRange changedRange;
        if (panelManager.ConsumePlaybackFrameRangeChange(changedRange)) {
            const int currentFrame = viewer->GetInfo().animTime * 30.0f + 0.5f;
            const int rangedFrame = std::clamp(currentFrame, changedRange.start, changedRange.end);
            if (rangedFrame != currentFrame)
                cameraManager.SeekFrame(viewer->GetInfo(), music, rangedFrame, saveTime);
        }
        int seekFrame = 0;
        bool seekFinished = false;
        if (panelManager.ConsumeSeekFrame(seekFrame, seekFinished)) {
            viewer->GetInfo().skipPhysics = !seekFinished;
            cameraManager.SeekFrame(viewer->GetInfo(), music, seekFrame, saveTime);
            if (seekFinished) {
                SyncSeekedPhysics(seekFrame);
                viewer->GetInfo().skipPhysics = false;
            }
        }
        switch (panelManager.ConsumePlaybackCommand()) {
            case PlaybackCommand::Play:
                viewer->GetInfo().skipPhysics = false;
                if (const auto [start, end] = panelManager.GetPlaybackFrameRange();
                    viewer->GetInfo().animTime * 30.0f < start || viewer->GetInfo().animTime * 30.0f >= end) {
                    cameraManager.SeekFrame(viewer->GetInfo(), music, start, saveTime);
                    SyncSeekedPhysics(start);
                }
                cameraManager.Play(music);
                break;
            case PlaybackCommand::Pause:
                cameraManager.Pause(music);
                break;
            case PlaybackCommand::Stop:
                viewer->GetInfo().skipPhysics = false;
                cameraManager.Stop(viewer->GetInfo(), music, saveTime);
                cameraManager.SeekFrame(viewer->GetInfo(), music, 0, saveTime);
                SyncSeekedPhysics(0);
                break;
            case PlaybackCommand::None:
                break;
        }
        inputManager.Update(viewer->GetInfo());
        cameraManager.HandleInput(inputManager, viewer->GetInfo(), music);
        if (!UpdateFramebufferSize())
            return false;
        cameraManager.StepTime(viewer->GetInfo(), music, saveTime);
        const int endFrame = panelManager.GetPlaybackFrameRange().end;
        const float playbackFrame = viewer->GetInfo().animTime * 30.0f;
        if (cameraManager.IsPlaying() && playbackFrame >= endFrame) {
            if (panelManager.IsPlaybackRepeatEnabled()) {
                const int startFrame = panelManager.GetPlaybackFrameRange().start;
                cameraManager.SeekFrame(viewer->GetInfo(), music, startFrame, saveTime);
                SyncSeekedPhysics(startFrame);
            } else {
                cameraManager.SeekFrame(viewer->GetInfo(), music, endFrame, saveTime);
                cameraManager.Pause(music);
                SyncSeekedPhysics(endFrame);
            }
            viewer->GetInfo().skipPhysics = false;
        }
        panelManager.SetPlaybackFrame(viewer->GetInfo().animTime * 30.0f + 0.5f);
        cameraManager.UpdateCamera(viewer->GetInfo());
        viewer->SetFpsVisible(panelManager.IsFpsVisible());
        viewer->BeginFrame();
        for (const auto& instance : instances) {
            instance->UpdateAnimation(viewer->GetInfo());
            instance->Update();
            instance->Draw();
        }
        if (!viewer->EndFrame())
            return false;
        TickFps();
        return true;
    }

    bool Program::Run() {
        Language::Initialize();
        CreateViewer(RendererType::OpenGL);
        const SceneConfig cfg;
        cameraManager.Reset();
        inputManager.Reset();
        panelManager.Reset();
        panelManager.ApplySceneConfig(cfg);
        if (!InitializeViewer()) {
            std::cerr << "Failed to run.\n";
            return false;
        }
        panelManager.BindSound(music);
        panelManager.OpenGuiWindows();
        panelManager.SetFrameLimits(CalculatePlaybackLastFrame(), CalculateMotionLastFrame());
        fpsTime = std::chrono::steady_clock::now();
        saveTime = std::chrono::steady_clock::now();
        fpsFrame = 0;
        LoadScene(cfg);
        cameraManager.UpdateCamera(viewer->GetInfo());
        while (!panelManager.IsCloseRequested() && !glfwWindowShouldClose(viewer->GetInfo().window)) {
            if (!RunFrame())
                break;
        }
        Shutdown();
        return true;
    }
}
