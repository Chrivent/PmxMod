#include "Viewer.h"

#define	STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#include <iostream>

#include "../src/MMDUtil.h"

unsigned char* AppContext::LoadImageRGBA(const std::filesystem::path& texturePath, int& x, int& y, int& comp) {
    stbi_uc* image = nullptr;
    x = y = comp = 0;
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, texturePath.c_str(), L"rb") != 0 || !fp)
        return nullptr;
    image = stbi_load_from_file(fp, &x, &y, &comp, STBI_rgb_alpha);
    std::fclose(fp);
    return image;
}

void AppContext::TickFps(std::chrono::steady_clock::time_point& fpsTime, int& fpsFrame) {
    fpsFrame++;
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsTime).count();
    if (sec > 1.0) {
        std::cout << (fpsFrame / sec) << " fps\n";
        fpsFrame = 0;
        fpsTime = std::chrono::steady_clock::now();
    }
}

void AppContext::LoadCameraVmd(const SceneConfig& cfg) {
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

void AppContext::StepTime(MusicUtil& music, std::chrono::steady_clock::time_point& saveTime) {
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

void AppContext::UpdateCamera(const int width, const int height) {
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
