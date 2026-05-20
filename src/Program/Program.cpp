#include "Program.h"

#include "../Animation/Model/Animation.h"
#include "../Animation/Model/AnimationBuilder.h"
#include "../Model/ModelLoader.h"
#include "../Model/ModelAnimator.h"
#include "../Parser/VmdParser.h"
#include "../Viewer/Dx11/Dx11Viewer.h"
#include "../Viewer/Glfw/GlfwViewer.h"

#include <iostream>

namespace Chrivent {
    void Program::CreateViewer(const int engineType) {
        if (engineType == 0)
            viewer = std::make_unique<GlfwViewer>();
        else if (engineType == 1)
            viewer = std::make_unique<Dx11Viewer>();
        else
            viewer.reset();
    }

    bool Program::InitializeViewer() {
        if (!viewer)
            return false;
        if (!glfwInit()) {
            std::cout << "Failed to initialize GLFW.\n";
            return false;
        }
        viewer->ConfigureGlfwHints();
        viewer->GetInfo().window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
        if (!viewer->GetInfo().window) {
            std::cout << "Failed to create viewer window.\n";
            viewer.reset();
            glfwTerminate();
            return false;
        }
        glfwGetFramebufferSize(viewer->GetInfo().window, &viewer->GetInfo().screenWidth, &viewer->GetInfo().screenHeight);
        if (viewer->GetInfo().screenWidth <= 0 || viewer->GetInfo().screenHeight <= 0) {
            std::cout << "Invalid framebuffer size.\n";
            viewer.reset();
            glfwTerminate();
            return false;
        }
        if (!viewer->Setup()) {
            std::cout << "Failed to set up renderer.\n";
            viewer.reset();
            glfwTerminate();
            return false;
        }
        return true;
    }

    void Program::Shutdown() {
        ClearInstances();
        music.Stop();
        panelManager.DestroyGui();
        viewer.reset();
        glfwTerminate();
    }

    bool Program::LoadScene(const SceneConfig& sceneConfig) {
        std::vector<std::unique_ptr<Instance>> loadedInstances;
        if (!LoadInstances(sceneConfig, loadedInstances)) {
            std::cout << "Failed to load scene instances.\n";
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
                std::cout << "Failed to load pmx file.\n";
                return false;
            }
            instance->GetInfo().model = pmxModel;
            const ModelAnimator animator(*instance->GetInfo().model);
            animator.InitializeAnimation();
            auto vmdAnim = std::make_unique<Animation>();
            AnimationInfo animationInfo;
            animationInfo.model = instance->GetInfo().model;
            const AnimationBuilder animationBuilder(animationInfo);
            for (const auto& vmdPath : animPaths) {
                VmdParser vmd;
                if (!vmd.ReadFile(vmdPath.c_str())) {
                    std::cout << "Failed to read VMD file.\n";
                    return false;
                }
                if (!animationBuilder.Add(vmd.GetData())) {
                    std::cout << "Failed to add VMDAnimation.\n";
                    return false;
                }
            }
            vmdAnim->SetInfo(std::move(animationInfo));
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
        int lastFrame = cameraManager.GetLastFrame();
        if (music.HasSound())
            lastFrame = (std::max)(lastFrame, static_cast<int>(std::ceil(music.GetLengthSeconds() * 30.0)));
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
            animator.SyncPhysics(*instance->GetInfo().anim, static_cast<float>(frame));
        }
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
        if (panelManager.ConsumeSceneConfigDirty())
            LoadScene(panelManager.GetSceneConfig());
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
                cameraManager.Play(music);
                break;
            case PlaybackCommand::Pause:
                cameraManager.Pause(music);
                break;
            case PlaybackCommand::Stop:
                viewer->GetInfo().skipPhysics = false;
                cameraManager.Stop(viewer->GetInfo(), music, saveTime);
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
        panelManager.SetPlaybackFrame(static_cast<int>(viewer->GetInfo().animTime * 30.0f + 0.5f));
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
        int engineType = 0;
        if (!(std::cin >> engineType)) {
            std::cin.clear();
            engineType = 0;
        }
        CreateViewer(engineType);
        const SceneConfig cfg;
        cameraManager.Reset();
        inputManager.Reset();
        panelManager.Reset();
        panelManager.ApplySceneConfig(cfg);
        if (!InitializeViewer()) {
            std::cout << "Failed to run.\n";
            return false;
        }
        panelManager.BindSound(music);
        panelManager.AttachRenderWindow(viewer->GetInfo());
        panelManager.OpenGuiWindows();
        fpsTime = std::chrono::steady_clock::now();
        saveTime = std::chrono::steady_clock::now();
        fpsFrame = 0;
        LoadScene(cfg);
        cameraManager.UpdateCamera(viewer->GetInfo());
        while (!glfwWindowShouldClose(viewer->GetInfo().window)) {
            if (!RunFrame())
                break;
        }
        Shutdown();
        return true;
    }
}
