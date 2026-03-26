#include "Viewer.h"

#include "../src/Model.h"
#include "../src/Sound.h"

#define	STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#include <iostream>
#include <windows.h>

void TickFps(std::chrono::steady_clock::time_point& fpsTime, int& fpsFrame) {
    fpsFrame++;
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsTime).count();
    if (sec > 1.0) {
        std::cout << (fpsFrame / sec) << " fps\n";
        fpsFrame = 0;
        fpsTime = std::chrono::steady_clock::now();
    }
}

void Instance::UpdateAnimation(const Viewer& viewer) const {
    m_model->BeginAnimation();
    m_model->UpdateAllAnimation(m_anim.get(), viewer.m_animTime * 30.0f, viewer.m_elapsed);
}

bool Viewer::Run(const SceneConfig& cfg) {
    Sound music;
    music.Init(cfg.m_musicPath, false);
    if (!glfwInit())
        return false;
    ConfigureGlfwHints();
    m_window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        return false;
    }
    glfwGetFramebufferSize(m_window, &m_screenWidth, &m_screenHeight);
    if (m_screenWidth <= 0 || m_screenHeight <= 0) {
        glfwTerminate();
        return false;
    }
    if (!Setup()) {
        glfwTerminate();
        return false;
    }
    LoadCameraAnim(cfg);
    std::vector<std::unique_ptr<Instance>> instances;
    if (!LoadInstances(cfg, instances)) {
        glfwTerminate();
        return false;
    }
    auto fpsTime  = std::chrono::steady_clock::now();
    auto saveTime = std::chrono::steady_clock::now();
    int fpsFrame  = 0;
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        int newW = 0, newH = 0;
        glfwGetFramebufferSize(m_window, &newW, &newH);
        if (newW != m_screenWidth || newH != m_screenHeight) {
            m_screenWidth = newW;
            m_screenHeight = newH;
            if (!Resize())
                break;
        }
        StepTime(music, saveTime);
        UpdateCamera();
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
    for (const auto& instance : instances)
        instance->Clear();
    instances.clear();
    glfwTerminate();
    return true;
}

unsigned char* Viewer::LoadImageRGBA(const std::filesystem::path& texturePath, int& x, int& y, int& comp, const bool flipY) {
    stbi_set_flip_vertically_on_load(flipY);
    stbi_uc* image = nullptr;
    x = y = comp = 0;
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, texturePath.c_str(), L"rb") != 0 || !fp)
        return nullptr;
    image = stbi_load_from_file(fp, &x, &y, &comp, STBI_rgb_alpha);
    std::fclose(fp);
    return image;
}

bool Viewer::LoadInstances(const SceneConfig& cfg, std::vector<std::unique_ptr<Instance>>& instances) {
    instances.clear();
    instances.reserve(cfg.m_modelConfigs.size());
    for (const auto& [modelPath, vmdPaths, scale] : cfg.m_modelConfigs) {
        auto instance = CreateInstance();
        const auto pmxModel = std::make_shared<Model>();
        if (!pmxModel->Load(modelPath, m_pmxDir)) {
            std::cout << "Failed to load pmx file.\n";
            return false;
        }
        instance->m_model = pmxModel;
        instance->m_model->InitializeAnimation();
        auto vmdAnim = std::make_unique<Animation>();
        vmdAnim->m_model = instance->m_model;
        for (const auto& vmdPath : vmdPaths) {
            VMDReader vmd;
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
        instance->m_anim = std::move(vmdAnim);
        instance->m_scale = scale;
        if (!instance->Setup(*this))
            return false;
        instances.emplace_back(std::move(instance));
    }
    return true;
}

void Viewer::LoadCameraAnim(const SceneConfig& cfg) {
    m_cameraAnim.reset();
    if (cfg.m_cameraAnim.empty()) {
        std::cout << "No camera VMD file.\n";
        return;
    }
    VMDReader camVmd;
    if (camVmd.ReadFile(cfg.m_cameraAnim.c_str()) && !camVmd.m_cameras.empty()) {
        auto vmdCamAnim = std::make_unique<CameraAnimation>();
        if (!vmdCamAnim->Create(camVmd))
            std::cout << "Failed to create VMDCameraAnimation.\n";
        m_cameraAnim = std::move(vmdCamAnim);
    }
}

void Viewer::StepTime(Sound& music, std::chrono::steady_clock::time_point& saveTime) {
    const auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - saveTime).count();
    if (elapsed > 1.0 / 30.0)
        elapsed = 1.0 / 30.0;
    saveTime = now;
    auto dt = static_cast<float>(elapsed);
    float t = m_animTime + dt;
    if (music.m_hasSound) {
        auto [adt, at] = music.PullTimes();
        if (adt < 0.f)
            adt = 0.f;
        dt = adt;
        t  = at;
    }
    m_elapsed  = dt;
    m_animTime = t;
}

void Viewer::UpdateCamera() {
    if (m_cameraAnim) {
        m_cameraAnim->Evaluate(m_animTime * 30.0f);
        const auto cam = m_cameraAnim->m_camera;
        m_viewMat = cam.GetViewMatrix();
        m_projMat = glm::perspectiveFovRH(
            cam.m_fov, static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight), 1.0f, 10000.0f
        );
        return;
    }
    m_viewMat = glm::lookAt(glm::vec3(0,10,40), glm::vec3(0,10,0), glm::vec3(0,1,0));
    m_projMat = glm::perspectiveFovRH(
        glm::radians(30.0f), static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight), 1.0f, 10000.0f
    );
}

void Viewer::InitDirs(const std::filesystem::path& shaderSubDir) {
    std::vector<wchar_t> buf(MAX_PATH);
    while (true) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n < buf.size() - 1) {
            m_resourceDir = std::filesystem::path(std::wstring(buf.data(), n));
            break;
        }
        buf.resize(buf.size() * 2);
    }
    m_resourceDir = m_resourceDir.parent_path() / "resource";
    m_shaderDir = m_resourceDir / shaderSubDir;
    m_pmxDir = m_resourceDir / "mmd";
}
