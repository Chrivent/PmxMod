#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>

#include "../src/Animation.h"

struct Sound;
struct SceneConfig;
struct Material;
struct ma_engine;
struct ma_sound;
struct Viewer;
class Model;
class Animation;

struct ModelConfig {
    std::filesystem::path				m_modelPath;
    std::vector<std::filesystem::path>	m_animPaths;
    float								m_scale = 1.1f;
};

struct SceneConfig {
    std::vector<ModelConfig>    m_modelConfigs;
    std::filesystem::path	    m_cameraAnim;
    std::filesystem::path	    m_musicPath;
};

struct Instance {
    virtual ~Instance() = default;

    std::shared_ptr<Model>	m_model;
    std::unique_ptr<Animation>	m_anim;
    float m_scale = 1.0f;

    virtual bool Setup(Viewer& viewer) = 0;
    virtual void Update() const = 0;
    virtual void Draw() const = 0;
    virtual void Clear() {}

    void UpdateAnimation(const Viewer& viewer) const;
};

struct Sound {
    Sound();
    ~Sound();

    bool Init(const std::filesystem::path& path);
    std::pair<float, float> PullTimes();

    bool m_hasSound = false;
    float m_volume = 0.1f;

private:
    void Uninit();

    std::unique_ptr<ma_engine> m_engine;
    std::unique_ptr<ma_sound>  m_sound;
    double m_prevTimeSec = 0.0;
};

struct Viewer {
    virtual ~Viewer() = default;

    std::filesystem::path	m_resourceDir;
    std::filesystem::path	m_shaderDir;
    std::filesystem::path	m_pmxDir;
    glm::mat4	m_viewMat;
    glm::mat4	m_projMat;
    int			m_screenWidth = 0;
    int			m_screenHeight = 0;
    glm::vec3	m_lightColor = glm::vec3(1, 1, 1);
    glm::vec3	m_lightDir = glm::vec3(-0.5f, -1.0f, -0.5f);
    float	m_elapsed = 0.0f;
    float	m_animTime = 0.0f;
    std::unique_ptr<CameraAnimation>	m_cameraAnim;

    float m_clearColor[4] = { 0.839f, 0.902f, 0.961f, 1.0f };
    GLFWwindow* m_window = nullptr;

    bool Run(const SceneConfig& cfg);

    virtual void ConfigureGlfwHints() = 0;
    virtual bool Setup() = 0;
    virtual bool Resize() = 0;
    virtual void BeginFrame() = 0;
    virtual bool EndFrame() = 0;
    virtual std::unique_ptr<Instance> CreateInstance() const = 0;

    static unsigned char* LoadImageRGBA(const std::filesystem::path& texturePath, int& x, int& y, int& comp, bool flipY = false);
    static void TickFps(std::chrono::steady_clock::time_point& fpsTime, int& fpsFrame);
    bool LoadInstances(const SceneConfig& cfg, std::vector<std::unique_ptr<Instance>>& instances);
    void LoadCameraAnim(const SceneConfig& cfg);
    void StepTime(Sound& music, std::chrono::steady_clock::time_point& saveTime);
    void UpdateCamera();
    void InitDirs(const std::filesystem::path& shaderSubDir);
};
