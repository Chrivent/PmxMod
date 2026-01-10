#include "Viewer.h"

#define	STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#include <iostream>

#include "../src/MMDUtil.h"
#include "../src/MMDModel.h"

void Model::UpdateAnimation(const Viewer& viewer) const {
    m_mmdModel->BeginAnimation();
    m_mmdModel->UpdateAllAnimation(m_vmdAnim.get(), viewer.m_animTime * 30.0f, viewer.m_elapsed);
}

unsigned char* Viewer::LoadImageRGBA(const std::filesystem::path& texturePath, int& x, int& y, int& comp) {
    stbi_uc* image = nullptr;
    x = y = comp = 0;
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, texturePath.c_str(), L"rb") != 0 || !fp)
        return nullptr;
    image = stbi_load_from_file(fp, &x, &y, &comp, STBI_rgb_alpha);
    std::fclose(fp);
    return image;
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

bool Viewer::LoadModels(const SceneConfig& cfg, std::vector<std::unique_ptr<Model>>& models) {
    models.clear();
    models.reserve(cfg.models.size());
    for (const auto& [modelPath, vmdPaths, scale] : cfg.models) {
        auto model = CreateModel();
        const auto ext = PathUtil::GetExt(modelPath);
        if (ext != "pmx") {
            std::cout << "Unknown file type. [" << ext << "]\n";
            return false;
        }
        const auto pmxModel = std::make_shared<MMDModel>();
        if (!pmxModel->Load(modelPath, m_mmdDir)) {
            std::cout << "Failed to load pmx file.\n";
            return false;
        }
        model->m_mmdModel = pmxModel;
        model->m_mmdModel->InitializeAnimation();
        auto vmdAnim = std::make_unique<VMDAnimation>();
        vmdAnim->m_model = model->m_mmdModel;
        for (const auto& vmdPath : vmdPaths) {
            VMDReader vmd;
            if (!vmd.ReadVMDFile(vmdPath.c_str())) {
                std::cout << "Failed to read VMD file.\n";
                return false;
            }
            if (!vmdAnim->Add(vmd)) {
                std::cout << "Failed to add VMDAnimation.\n";
                return false;
            }
        }
        vmdAnim->SyncPhysics(0.0f);
        model->m_vmdAnim = std::move(vmdAnim);
        model->m_scale = scale;
        if (!model->Setup(*this))
            return false;
        models.emplace_back(std::move(model));
    }
    return true;
}

void Viewer::LoadCameraVmd(const SceneConfig& cfg) {
    m_vmdCameraAnim.reset();
    if (cfg.cameraVmd.empty())
        std::cout << "No camera VMD file.\n";
    VMDReader camVmd;
    if (camVmd.ReadVMDFile(cfg.cameraVmd.c_str()) && !camVmd.m_cameras.empty()) {
        auto vmdCamAnim = std::make_unique<VMDCameraAnimation>();
        if (!vmdCamAnim->Create(camVmd))
            std::cout << "Failed to create VMDCameraAnimation.\n";
        m_vmdCameraAnim = std::move(vmdCamAnim);
    }
}

void Viewer::StepTime(MusicUtil& music, std::chrono::steady_clock::time_point& saveTime) {
    const auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - saveTime).count();
    if (elapsed > 1.0 / 30.0)
        elapsed = 1.0 / 30.0;
    saveTime = now;
    auto dt = static_cast<float>(elapsed);
    float t = m_animTime + dt;
    if (music.HasMusic()) {
        auto [adt, at] = music.PullTimes();
        if (adt < 0.f) adt = 0.f;
        dt = adt;
        t  = at;
    }
    m_elapsed  = dt;
    m_animTime = t;
}

void Viewer::UpdateCamera(const int width, const int height) {
    m_screenWidth  = width;
    m_screenHeight = height;
    if (m_vmdCameraAnim) {
        m_vmdCameraAnim->Evaluate(m_animTime * 30.0f);
        const auto mmdCam = m_vmdCameraAnim->m_camera;
        m_viewMat = mmdCam.GetViewMatrix();
        m_projMat = glm::perspectiveFovRH(
            mmdCam.m_fov, static_cast<float>(width), static_cast<float>(height), 1.0f, 10000.0f
        );
        return;
    }
    m_viewMat = glm::lookAt(glm::vec3(0,10,40), glm::vec3(0,10,0), glm::vec3(0,1,0));
    m_projMat = glm::perspectiveFovRH(
        glm::radians(30.0f), static_cast<float>(width), static_cast<float>(height), 1.0f, 10000.0f
    );
}

void Viewer::InitDirs(const std::string& shaderSubDir) {
    m_resourceDir = PathUtil::GetExecutablePath();
    m_resourceDir = m_resourceDir.parent_path();
    m_resourceDir /= "resource";
    m_shaderDir = m_resourceDir / shaderSubDir;
    m_mmdDir = m_resourceDir / "mmd";
}
