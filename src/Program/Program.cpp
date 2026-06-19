#include "Program.h"

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
        if (!LoadScene(panelManager.GetSceneConfig()))
            return false;
        panelManager.BindSound(music);
        panelManager.SetPlaybackFrameRange(CalculatePlaybackLastFrame());
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

    bool Program::LoadScene(const SceneConfig& sceneConfig) {
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
        panelManager.SetPlaybackFrameRange(CalculatePlaybackLastFrame());
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

    int Program::CalculatePlaybackLastFrame() const {
        uint32_t lastFrame = cameraManager.GetLastFrame();
        if (music.HasSound())
            lastFrame = (std::max)(lastFrame, static_cast<uint32_t>(std::ceil(music.GetLengthSeconds() * 30.0)));
        for (const auto& instance : instances) {
            if (instance && instance->GetInfo().anim)
                lastFrame = (std::max)(lastFrame, instance->GetInfo().anim->GetLastFrame());
        }
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
        std::unordered_map<const Node*, std::vector<uint32_t>> nodeKeyFrames;
        std::unordered_map<const IkSolver*, std::vector<uint32_t>> ikKeyFrames;
        std::unordered_map<const Morph*, std::vector<uint32_t>> morphKeyFrames;
        if (instanceInfo.anim) {
            const auto& [nodeTracks, ikTracks, morphTracks] = instanceInfo.anim->GetInfo();
            for (const auto& [node, keys] : nodeTracks) {
                auto& frames = nodeKeyFrames[node.get()];
                frames.reserve(keys.size());
                for (const auto& key : keys)
                    frames.emplace_back(key.frame);
            }
            for (const auto& [ikSolver, keys] : ikTracks) {
                auto& frames = ikKeyFrames[ikSolver.get()];
                frames.reserve(keys.size());
                for (const auto& [frame, ikEnable] : keys)
                    frames.emplace_back(frame);
            }
            for (const auto& [morph, keys] : morphTracks) {
                auto& frames = morphKeyFrames[morph.get()];
                frames.reserve(keys.size());
                for (const auto& [frame, morphWeight] : keys)
                    frames.emplace_back(frame);
            }
        }
        std::vector<MotionTimelineRow> rows;
        rows.reserve(model.skeletonData.nodes.size() + model.skeletonData.ikSolvers.size() + model.morphData.morphs.size());
        for (const auto& node : model.skeletonData.nodes) {
            if (!node)
                continue;
            rows.push_back({
                .name = Util::Utf8ToWString(node->GetInfo().name),
                .keyFrames = nodeKeyFrames[node.get()]
            });
        }
        for (const auto& ikSolver : model.skeletonData.ikSolvers) {
            if (!ikSolver)
                continue;
            const auto ikNode = ikSolver->GetInfo().ikNode.lock();
            if (!ikNode)
                continue;
            rows.push_back({
                .name = L"IK: " + Util::Utf8ToWString(ikNode->GetInfo().name),
                .keyFrames = ikKeyFrames[ikSolver.get()]
            });
        }
        for (const auto& morph : model.morphData.morphs) {
            if (!morph)
                continue;
            rows.push_back({
                .name = L"모프: " + Util::Utf8ToWString(morph->name),
                .keyFrames = morphKeyFrames[morph.get()]
            });
        }
        std::wstring modelName = Util::Utf8ToWString(model.infoData.modelName);
        if (modelName.empty() && modelIndex < panelManager.GetSceneConfig().modelConfigs.size())
            modelName = panelManager.GetSceneConfig().modelConfigs[modelIndex].modelPath.filename().wstring();
        panelManager.SetMotionTimeline(std::move(modelName), std::move(rows));
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
            std::cout << (fpsFrame / sec) << " fps\n";
            fpsFrame = 0;
            fpsTime = std::chrono::steady_clock::now();
        }
    }

    bool Program::RunFrame() {
        glfwPollEvents();
        panelManager.PollGuiWindows();
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
                if (const int startFrame = panelManager.GetPlaybackFrameRange().start; startFrame > 0)
                    cameraManager.SeekFrame(viewer->GetInfo(), music, startFrame, saveTime);
                SyncSeekedPhysics(panelManager.GetPlaybackFrameRange().start);
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
        if (viewer->GetInfo().elapsed > 0.0f && playbackFrame >= endFrame) {
            cameraManager.SeekFrame(viewer->GetInfo(), music, endFrame, saveTime);
            cameraManager.Pause(music);
            SyncSeekedPhysics(endFrame);
            viewer->GetInfo().skipPhysics = false;
        }
        panelManager.SetPlaybackFrame(viewer->GetInfo().animTime * 30.0f + 0.5f);
        cameraManager.UpdateCamera(viewer->GetInfo());
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
        panelManager.SetPlaybackFrameRange(CalculatePlaybackLastFrame());
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
