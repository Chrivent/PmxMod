#include "Viewer.h"

#include "../src/Model.h"
#include "../src/Sound.h"

#include <iostream>
#include <windows.h>

#define	STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Instance::~Instance() = default;

void Instance::Draw() const {
    DrawModel();
    DrawEdge();
    DrawGroundShadow();
}

void Instance::UpdateAnimation(const Viewer& viewer) const {
    model->BeginAnimation();
    model->UpdateAllAnimation(anim.get(), viewer.animTime * 30.0f, viewer.elapsed);
}

void Viewer::TickFps(std::chrono::steady_clock::time_point& fpsTime, int& fpsFrame) {
    fpsFrame++;
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsTime).count();
    if (sec > 1.0) {
        std::cout << (fpsFrame / sec) << " fps\n";
        fpsFrame = 0;
        fpsTime = std::chrono::steady_clock::now();
    }
}

Viewer::~Viewer() = default;

bool Viewer::Run(const SceneConfig& cfg) {
    Sound music;
    controller.Reset();
    controller.ApplySceneConfig(cfg);
    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW.\n";
        return false;
    }
    ConfigureGlfwHints();
    window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create viewer window.\n";
        glfwTerminate();
        return false;
    }
    glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
    if (screenWidth <= 0 || screenHeight <= 0) {
        std::cout << "Invalid framebuffer size.\n";
        glfwTerminate();
        return false;
    }
    if (!Setup()) {
        std::cout << "Failed to set up renderer.\n";
        glfwTerminate();
        return false;
    }
    controller.OpenControlWindow();
    std::vector<std::unique_ptr<Instance>> instances;
    auto fpsTime  = std::chrono::steady_clock::now();
    auto saveTime = std::chrono::steady_clock::now();
    int fpsFrame  = 0;
    auto clearInstances = [](const std::vector<std::unique_ptr<Instance>>& targetInstances) {
        for (const auto& instance : targetInstances)
            instance->Clear();
    };
    auto loadScene = [&](const SceneConfig& sceneConfig) {
        std::vector<std::unique_ptr<Instance>> loadedInstances;
        if (!LoadInstances(sceneConfig, loadedInstances)) {
            std::cout << "Failed to load scene instances.\n";
            return false;
        }
        clearInstances(instances);
        instances = std::move(loadedInstances);
        music.Stop();
        music.Init(sceneConfig.musicPath, false);
        controller.LoadCameraAnim(sceneConfig.cameraAnim);
        elapsed = 0.0f;
        animTime = 0.0f;
        saveTime = std::chrono::steady_clock::now();
        return true;
    };
    if (!cfg.modelConfigs.empty() || !cfg.cameraAnim.empty() || !cfg.musicPath.empty())
        loadScene(cfg);
    controller.UpdateCamera(*this);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        controller.PollControlWindow();
        if (controller.ConsumeSceneConfigDirty())
            loadScene(controller.sceneConfig);
        controller.HandleInput(*this, music);
        int newW = 0, newH = 0;
        glfwGetFramebufferSize(window, &newW, &newH);
        if (newW != screenWidth || newH != screenHeight) {
            screenWidth = newW;
            screenHeight = newH;
            if (!Resize())
                break;
        }
        controller.StepTime(*this, music, saveTime);
        controller.UpdateCamera(*this);
        BeginFrame();
        for (const auto& instance : instances) {
            instance->UpdateAnimation(*this);
            instance->Update();
            instance->Draw();
        }
        if (!EndFrame())
            break;
        TickFps(fpsTime, fpsFrame);
    }
    clearInstances(instances);
    controller.DestroyControlWindow();
    glfwTerminate();
    return true;
}

unsigned char* Viewer::LoadImageRgba(const std::filesystem::path& texturePath, int& x, int& y, int& comp, const bool flipY) {
    stbi_set_flip_vertically_on_load(flipY);
    x = y = comp = 0;
    FILE* imageFile = nullptr;
    if (_wfopen_s(&imageFile, texturePath.c_str(), L"rb") != 0 || !imageFile)
        return nullptr;
    stbi_uc* image = stbi_load_from_file(imageFile, &x, &y, &comp, STBI_rgb_alpha);
    std::fclose(imageFile);
    return image;
}

bool Viewer::LoadInstances(const SceneConfig& cfg, std::vector<std::unique_ptr<Instance>>& instances) {
    instances.clear();
    instances.reserve(cfg.modelConfigs.size());
    for (const auto& [modelPath, animPaths, scale] : cfg.modelConfigs) {
        auto instance = CreateInstance();
        const auto pmxModel = std::make_shared<Model>();
        if (!pmxModel->Load(modelPath, pmxDir)) {
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
        if (!instance->Setup(*this))
            return false;
        instances.emplace_back(std::move(instance));
    }
    return true;
}

void Viewer::InitDirs(const std::filesystem::path& shaderSubDir) {
    std::vector<wchar_t> buf(MAX_PATH);
    while (true) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n < buf.size() - 1) {
            resourceDir = std::filesystem::path(std::wstring(buf.data(), n));
            break;
        }
        buf.resize(buf.size() * 2);
    }
    resourceDir = resourceDir.parent_path() / "resource";
    shaderDir = resourceDir / shaderSubDir;
    pmxDir = resourceDir / "mmd";
}

