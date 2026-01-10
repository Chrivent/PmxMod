#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>

#include "../src/VMDAnimation.h"

struct MusicUtil;
struct SceneConfig;
struct MMDMaterial;
class MMDModel;
class VMDAnimation;

struct AppContext {
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

    static unsigned char* LoadImageRGBA(const std::filesystem::path& texturePath, int& x, int& y, int& comp);
    static void TickFps(std::chrono::steady_clock::time_point& fpsTime, int& fpsFrame);

    void LoadCameraVmd(const SceneConfig& cfg);
    void StepTime(MusicUtil& music, std::chrono::steady_clock::time_point& saveTime);
    void UpdateCamera(int width, int height);
};

struct Model {
    std::shared_ptr<MMDModel>	m_mmdModel;
    std::unique_ptr<VMDAnimation>	m_vmdAnim;
    float m_scale = 1.0f;
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
