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
    m_paused = false;
    m_prevSpaceDown = false;
    m_useMotionCamera = true;
    m_prevRDown = false;
    m_prevRightMouseDown = false;
    m_freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
    m_freeCamYaw = glm::radians(-90.0f);
    m_freeCamPitch = 0.0f;
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
    UpdateCamera();
    SyncFreeCameraToCurrentView();
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        HandleInput(music);
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

void Viewer::HandleInput(Sound& music) {
    const bool spaceDown = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spaceDown && !m_prevSpaceDown) {
        m_paused = !m_paused;
        if (m_paused)
            music.Pause();
        else
            music.Resume();
    }
    m_prevSpaceDown = spaceDown;

    const bool rDown = glfwGetKey(m_window, GLFW_KEY_R) == GLFW_PRESS;
    if (rDown && !m_prevRDown) {
        if (m_useMotionCamera)
            SyncFreeCameraToCurrentView();
        m_useMotionCamera = !m_useMotionCamera;
        m_prevRightMouseDown = false;
    }
    m_prevRDown = rDown;

    if (m_useMotionCamera)
        return;

    const float moveSpeed = 100.0f * max(m_elapsed, 1.0f / 120.0f);
    glm::vec3 forward(
        std::cos(m_freeCamPitch) * std::cos(m_freeCamYaw),
        std::sin(m_freeCamPitch),
        std::cos(m_freeCamPitch) * std::sin(m_freeCamYaw)
    );
    forward = glm::normalize(forward);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);

    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS)
        m_freeCamPosition += forward * moveSpeed;
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)
        m_freeCamPosition -= forward * moveSpeed;
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
        m_freeCamPosition -= right * moveSpeed;
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
        m_freeCamPosition += right * moveSpeed;
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS)
        m_freeCamPosition -= up * moveSpeed;
    if (glfwGetKey(m_window, GLFW_KEY_E) == GLFW_PRESS)
        m_freeCamPosition += up * moveSpeed;

    const bool rightMouseDown = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    double cursorX = 0.0, cursorY = 0.0;
    glfwGetCursorPos(m_window, &cursorX, &cursorY);
    if (rightMouseDown) {
        if (m_prevRightMouseDown) {
            constexpr float mouseSensitivity = 0.0035f;
            const double dx = cursorX - m_prevCursorX;
            const double dy = cursorY - m_prevCursorY;
            m_freeCamYaw += static_cast<float>(dx) * mouseSensitivity;
            m_freeCamPitch -= static_cast<float>(dy) * mouseSensitivity;
            m_freeCamPitch = std::clamp(m_freeCamPitch, glm::radians(-89.0f), glm::radians(89.0f));
        }
        m_prevCursorX = cursorX;
        m_prevCursorY = cursorY;
    }
    m_prevRightMouseDown = rightMouseDown;
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
    if (m_paused) {
        m_elapsed = 0.0f;
        return;
    }
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
    if (m_useMotionCamera && m_cameraAnim) {
        m_cameraAnim->Evaluate(m_animTime * 30.0f);
        const auto cam = m_cameraAnim->m_camera;
        m_viewMat = cam.GetViewMatrix();
        m_projMat = glm::perspectiveFovRH(
            cam.m_fov, static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight), 1.0f, 10000.0f
        );
        return;
    }
    glm::vec3 forward(
        std::cos(m_freeCamPitch) * std::cos(m_freeCamYaw),
        std::sin(m_freeCamPitch),
        std::cos(m_freeCamPitch) * std::sin(m_freeCamYaw)
    );
    forward = glm::normalize(forward);
    m_viewMat = glm::lookAt(m_freeCamPosition, m_freeCamPosition + forward, glm::vec3(0, 1, 0));
    m_projMat = glm::perspectiveFovRH(
        glm::radians(30.0f), static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight), 1.0f, 10000.0f
    );
}

void Viewer::SyncFreeCameraToCurrentView() {
    const glm::mat4 invView = glm::inverse(m_viewMat);
    m_freeCamPosition = glm::vec3(invView[3]);
    const glm::vec3 forward = -glm::normalize(glm::vec3(invView[2]));
    m_freeCamYaw = std::atan2(forward.z, forward.x);
    m_freeCamPitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
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
