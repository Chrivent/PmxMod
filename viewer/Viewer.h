#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>

#include "../src/VMDAnimation.h"

struct MusicUtil;
struct SceneConfig;
struct MMDMaterial;
struct Viewer;
class MMDModel;
class VMDAnimation;

struct Input {
    std::filesystem::path				m_modelPath;
    std::vector<std::filesystem::path>	m_vmdPaths;
    float								m_scale = 1.1f;
};

struct SceneConfig {
    std::vector<Input>		models;
    std::filesystem::path	cameraVmd;
    std::filesystem::path	musicPath;
};

struct Model {
    virtual ~Model() = default;

    std::shared_ptr<MMDModel>	m_mmdModel;
    std::unique_ptr<VMDAnimation>	m_vmdAnim;
    float m_scale = 1.0f;

    virtual bool Setup(Viewer& viewer) = 0;
    virtual void Update() const = 0;
    virtual void Draw(Viewer& viewer) const = 0;
    virtual void Clear() {}

    void UpdateAnimation(const Viewer& viewer) const;
};

struct Viewer {
    virtual ~Viewer() = default;

    std::filesystem::path	m_resourceDir;
    std::filesystem::path	m_shaderDir;
    std::filesystem::path	m_mmdDir;
    glm::mat4	m_viewMat;
    glm::mat4	m_projMat;
    int			m_screenWidth = 0;
    int			m_screenHeight = 0;
    glm::vec3	m_lightColor = glm::vec3(1, 1, 1);
    glm::vec3	m_lightDir = glm::vec3(-0.5f, -1.0f, -0.5f);
    float	m_elapsed = 0.0f;
    float	m_animTime = 0.0f;
    std::unique_ptr<VMDCameraAnimation>	m_vmdCameraAnim;

    float m_clearColor[4] = { 0.839f, 0.902f, 0.961f, 1.0f };
    GLFWwindow* m_window = nullptr;

    bool Run(const SceneConfig& cfg);

    virtual void ConfigureGlfwHints() = 0;
    virtual bool Setup() = 0;
    virtual bool Resize() = 0;
    virtual void BeginFrame() = 0;
    virtual bool EndFrame() = 0;
    virtual void AfterModelDraw(Model& model) {}
    virtual std::unique_ptr<Model> CreateModel() const = 0;

    static unsigned char* LoadImageRGBA(const std::filesystem::path& texturePath, int& x, int& y, int& comp, bool flipY = false);
    static void TickFps(std::chrono::steady_clock::time_point& fpsTime, int& fpsFrame);
    bool LoadModels(const SceneConfig& cfg, std::vector<std::unique_ptr<Model>>& models);
    void LoadCameraVmd(const SceneConfig& cfg);
    void StepTime(MusicUtil& music, std::chrono::steady_clock::time_point& saveTime);
    void UpdateCamera();
    void InitDirs(const std::string& shaderSubDir);
};
