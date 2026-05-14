#include "Program.h"

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

    bool Program::InitializeViewer() const {
        if (!viewer)
            return false;
        if (!glfwInit()) {
            std::cout << "Failed to initialize GLFW.\n";
            return false;
        }
        viewer->ConfigureGlfwHints();
        viewer->window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
        if (!viewer->window) {
            std::cout << "Failed to create viewer window.\n";
            glfwTerminate();
            return false;
        }
        glfwGetFramebufferSize(viewer->window, &viewer->screenWidth, &viewer->screenHeight);
        if (viewer->screenWidth <= 0 || viewer->screenHeight <= 0) {
            std::cout << "Invalid framebuffer size.\n";
            glfwTerminate();
            return false;
        }
        if (!viewer->Setup()) {
            std::cout << "Failed to set up renderer.\n";
            glfwTerminate();
            return false;
        }
        return true;
    }

    void Program::Shutdown() {
        ClearInstances();
        music.Stop();
        controller.DestroyControlWindow();
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
        controller.LoadCameraAnim(sceneConfig.cameraAnim);
        viewer->elapsed = 0.0f;
        viewer->animTime = 0.0f;
        saveTime = std::chrono::steady_clock::now();
        return true;
    }

    bool Program::LoadInstances(const SceneConfig& sceneConfig, std::vector<std::unique_ptr<Instance>>& loadedInstances) const {
        loadedInstances.clear();
        loadedInstances.reserve(sceneConfig.modelConfigs.size());
        for (const auto& [modelPath, animPaths, scale] : sceneConfig.modelConfigs) {
            auto instance = viewer->CreateInstance();
            const auto pmxModel = std::make_shared<Model>();
            if (!pmxModel->Load(modelPath, viewer->pmxDir)) {
                std::cout << "Failed to load pmx file.\n";
                return false;
            }
            instance->model = pmxModel;
            instance->model->InitializeAnimation();
            auto vmdAnim = std::make_unique<Animation>();
            vmdAnim->model = instance->model;
            for (const auto& vmdPath : animPaths) {
                VmdReader vmd;
                if (!vmd.ReadFile(vmdPath.c_str())) {
                    std::cout << "Failed to read VMD file.\n";
                    return false;
                }
                if (!vmdAnim->Add(vmd)) {
                    std::cout << "Failed to add VMDAnimation.\n";
                    return false;
                }
            }
            vmdAnim->SyncPhysics(0.0f);
            instance->anim = std::move(vmdAnim);
            instance->scale = scale;
            if (!instance->Setup(*viewer))
                return false;
            loadedInstances.emplace_back(std::move(instance));
        }
        return true;
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
            std::cout << (fpsFrame / sec) << " fps\n";
            fpsFrame = 0;
            fpsTime = std::chrono::steady_clock::now();
        }
    }

    bool Program::RunFrame() {
        glfwPollEvents();
        controller.PollControlWindow();
        if (controller.ConsumeSceneConfigDirty())
            LoadScene(controller.sceneConfig);
        controller.HandleInput(*viewer, music);
        if (!UpdateFramebufferSize())
            return false;
        controller.StepTime(*viewer, music, saveTime);
        controller.UpdateCamera(*viewer);
        viewer->BeginFrame();
        for (const auto& instance : instances) {
            instance->UpdateAnimation(*viewer);
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
        controller.Reset();
        controller.ApplySceneConfig(cfg);
        if (!InitializeViewer()) {
            std::cout << "Failed to run.\n";
            return false;
        }
        controller.OpenControlWindow();
        fpsTime = std::chrono::steady_clock::now();
        saveTime = std::chrono::steady_clock::now();
        fpsFrame = 0;
        LoadScene(cfg);
        controller.UpdateCamera(*viewer);
        while (!glfwWindowShouldClose(viewer->window)) {
            if (!RunFrame())
                break;
        }
        Shutdown();
        return true;
    }
}
