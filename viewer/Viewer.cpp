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
    music.Init(cfg.musicPath, false);
    paused = false;
    prevSpaceDown = false;
    useMotionCamera = true;
    hasFreeCameraState = false;
    prevRDown = false;
    prevRightMouseDown = false;
    freeCamPosition = glm::vec3(0.0f, 10.0f, 40.0f);
    freeCamYaw = glm::radians(-90.0f);
    freeCamPitch = 0.0f;
    if (!glfwInit())
        return false;
    ConfigureGlfwHints();
    window = glfwCreateWindow(1280, 720, "Pmx Mod", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return false;
    }
    glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
    if (screenWidth <= 0 || screenHeight <= 0) {
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
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        HandleInput(music);
        int newW = 0, newH = 0;
        glfwGetFramebufferSize(window, &newW, &newH);
        if (newW != screenWidth || newH != screenHeight) {
            screenWidth = newW;
            screenHeight = newH;
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
    glfwTerminate();
    return true;
}

unsigned char* Viewer::LoadImageRgba(const std::filesystem::path& texturePath, int& x, int& y, int& comp, const bool flipY) {
    stbi_set_flip_vertically_on_load(flipY);
    x = y = comp = 0;
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, texturePath.c_str(), L"rb") != 0 || !fp)
        return nullptr;
    stbi_uc* image = stbi_load_from_file(fp, &x, &y, &comp, STBI_rgb_alpha);
    std::fclose(fp);
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

void Viewer::LoadCameraAnim(const SceneConfig& cfg) {
    cameraAnim.reset();
    if (cfg.cameraAnim.empty()) {
        std::cout << "No camera VMD file.\n";
        return;
    }
    VmdReader camVmd;
    if (camVmd.ReadFile(cfg.cameraAnim.c_str()) && !camVmd.cameras.empty()) {
        auto vmdCamAnim = std::make_unique<CameraAnimation>();
        if (!vmdCamAnim->Create(camVmd))
            std::cout << "Failed to create VMDCameraAnimation.\n";
        cameraAnim = std::move(vmdCamAnim);
    }
}

void Viewer::StepTime(Sound& music, std::chrono::steady_clock::time_point& saveTime) {
    const auto now = std::chrono::steady_clock::now();
    double elapsedSeconds = std::chrono::duration<double>(now - saveTime).count();
    if (elapsedSeconds > 1.0 / 30.0)
        elapsedSeconds = 1.0 / 30.0;
    saveTime = now;
    if (paused) {
        elapsed = 0.0f;
        return;
    }
    auto dt = static_cast<float>(elapsedSeconds);
    float t = animTime + dt;
    if (music.hasSound) {
        float adt = 0.0f;
        float at = 0.0f;
        music.PullTimes(adt, at);
        if (adt < 0.f)
            adt = 0.f;
        dt = adt;
        t  = at;
    }
    elapsed  = dt;
    animTime = t;
}

void Viewer::HandleInput(Sound& music) {
    const bool spaceDown = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spaceDown && !prevSpaceDown) {
        paused = !paused;
        if (paused)
            music.Pause();
        else
            music.Resume();
    }
    prevSpaceDown = spaceDown;
    const bool rDown = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (rDown && !prevRDown) {
        if (useMotionCamera && !hasFreeCameraState) {
            SyncFreeCameraToCurrentView();
            hasFreeCameraState = true;
        }
        useMotionCamera = !useMotionCamera;
        prevRightMouseDown = false;
    }
    prevRDown = rDown;
    if (useMotionCamera)
        return;
    const float moveSpeed = 100.0f * max(elapsed, 1.0f / 120.0f);
    glm::vec3 forward(
        std::cos(freeCamPitch) * std::cos(freeCamYaw),
        std::sin(freeCamPitch),
        std::cos(freeCamPitch) * std::sin(freeCamYaw)
    );
    forward = glm::normalize(forward);
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        freeCamPosition += forward * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        freeCamPosition -= forward * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        freeCamPosition -= right * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        freeCamPosition += right * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        freeCamPosition -= up * moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        freeCamPosition += up * moveSpeed;
    const bool rightMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    double cursorX = 0.0, cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    if (rightMouseDown) {
        if (prevRightMouseDown) {
            constexpr float mouseSensitivity = 0.0035f;
            const double dx = cursorX - prevCursorX;
            const double dy = cursorY - prevCursorY;
            freeCamYaw += static_cast<float>(dx) * mouseSensitivity;
            freeCamPitch -= static_cast<float>(dy) * mouseSensitivity;
            freeCamPitch = std::clamp(freeCamPitch, glm::radians(-89.0f), glm::radians(89.0f));
        }
        prevCursorX = cursorX;
        prevCursorY = cursorY;
    }
    prevRightMouseDown = rightMouseDown;
}

void Viewer::UpdateCamera() {
    if (useMotionCamera && cameraAnim) {
        cameraAnim->Evaluate(animTime * 30.0f);
        const auto cam = cameraAnim->camera;
        viewMat = cam.CalcViewMatrix();
        projMat = glm::perspectiveFovRH(
            cam.fov, static_cast<float>(screenWidth), static_cast<float>(screenHeight), 1.0f, 10000.0f
        );
        return;
    }
    glm::vec3 forward(
        std::cos(freeCamPitch) * std::cos(freeCamYaw),
        std::sin(freeCamPitch),
        std::cos(freeCamPitch) * std::sin(freeCamYaw)
    );
    forward = glm::normalize(forward);
    viewMat = glm::lookAt(freeCamPosition, freeCamPosition + forward, glm::vec3(0, 1, 0));
    projMat = glm::perspectiveFovRH(
        glm::radians(30.0f), static_cast<float>(screenWidth), static_cast<float>(screenHeight), 1.0f, 10000.0f
    );
}

void Viewer::SyncFreeCameraToCurrentView() {
    const glm::mat4 invView = glm::inverse(viewMat);
    freeCamPosition = glm::vec3(invView[3]);
    const glm::vec3 forward = -glm::normalize(glm::vec3(invView[2]));
    freeCamYaw = std::atan2(forward.z, forward.x);
    freeCamPitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
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

