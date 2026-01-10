#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>

#include "../src/VMDAnimation.h"

struct MusicUtil;
struct SceneConfig;
struct MMDMaterial;
struct Model;
class MMDModel;
class VMDAnimation;

struct AppContext {
    virtual ~AppContext() = default;

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

    virtual bool Run(const SceneConfig& cfg) = 0;
    virtual std::unique_ptr<Model> CreateModel() const = 0;
    static unsigned char* LoadImageRGBA(const std::filesystem::path& texturePath, int& x, int& y, int& comp);
    static void TickFps(std::chrono::steady_clock::time_point& fpsTime, int& fpsFrame);
    bool LoadModels(const SceneConfig& cfg, std::vector<std::unique_ptr<Model>>& models);
    void LoadCameraVmd(const SceneConfig& cfg);
    void StepTime(MusicUtil& music, std::chrono::steady_clock::time_point& saveTime);
    void UpdateCamera(int width, int height);
};

struct Model {
    virtual ~Model() = default;

    std::shared_ptr<MMDModel>	m_mmdModel;
    std::unique_ptr<VMDAnimation>	m_vmdAnim;
    float m_scale = 1.0f;

    virtual bool Setup(AppContext& appContext) = 0;
    virtual void UpdateAnimation(const AppContext& appContext) const = 0;
    virtual void Update() const = 0;
    virtual void Draw(AppContext& appContext) const = 0;
    virtual void Clear() {}
};

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
