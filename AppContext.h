#pragma once

#include <glad/glad.h>

#include <map>

#include "src/VMDCameraAnimation.h"
#include "MMDShader.h"

struct Texture
{
	GLuint	m_texture;
	bool	m_hasAlpha;
};

struct AppContext
{
    ~AppContext();

    std::filesystem::path	m_resourceDir;
    std::filesystem::path	m_shaderDir;
    std::filesystem::path	m_mmdDir;

    std::unique_ptr<MMDShader>				m_mmdShader;
    std::unique_ptr<MMDEdgeShader>			m_mmdEdgeShader;
    std::unique_ptr<MMDGroundShadowShader>	m_mmdGroundShadowShader;

    glm::mat4	m_viewMat;
    glm::mat4	m_projMat;
    int			m_screenWidth = 0;
    int			m_screenHeight = 0;

    glm::vec3	m_lightColor = glm::vec3(1, 1, 1);
    glm::vec3	m_lightDir = glm::vec3(-0.5f, -1.0f, -0.5f);

    std::map<std::filesystem::path, Texture>	m_textures;
    GLuint	m_dummyColorTex = 0;
    GLuint	m_dummyShadowDepthTex = 0;

    const int	m_msaaSamples = 4;

    float	m_elapsed = 0.0f;
    float	m_animTime = 0.0f;
    std::unique_ptr<VMDCameraAnimation>	m_vmdCameraAnim;

    bool Setup();
    void Clear();

    Texture GetTexture(const std::filesystem::path& texturePath);
};
