#pragma once

#include <glad/glad.h>

#include <map>

#include "VMDAnimation.h"
#include "VMDCameraAnimation.h"

struct Texture
{
	GLuint	m_texture;
	bool	m_hasAlpha;
};

struct MMDShader;
struct MMDEdgeShader;
struct MMDGroundShadowShader;

struct AppContext
{
    ~AppContext();

    std::string m_resourceDir;
    std::string	m_shaderDir;
    std::string	m_mmdDir;

    std::unique_ptr<MMDShader>				m_mmdShader;
    std::unique_ptr<MMDEdgeShader>			m_mmdEdgeShader;
    std::unique_ptr<MMDGroundShadowShader>	m_mmdGroundShadowShader;

    glm::mat4	m_viewMat;
    glm::mat4	m_projMat;
    int			m_screenWidth = 0;
    int			m_screenHeight = 0;

    glm::vec3	m_lightColor = glm::vec3(1, 1, 1);
    glm::vec3	m_lightDir = glm::vec3(-0.5f, -1.0f, -0.5f);

    std::map<std::string, Texture>	m_textures;
    GLuint	m_dummyColorTex = 0;
    GLuint	m_dummyShadowDepthTex = 0;

    const int	m_msaaSamples = 4;

    bool		m_enableTransparentWindow = false;
    uint32_t	m_transparentFboWidth = 0;
    uint32_t	m_transparentFboHeight = 0;
    GLuint	m_transparentFboColorTex = 0;
    GLuint	m_transparentFbo = 0;
    GLuint	m_transparentFboMSAAColorRB = 0;
    GLuint	m_transparentFboMSAADepthRB = 0;
    GLuint	m_transparentMSAAFbo = 0;
    GLuint	m_copyTransparentWindowShader = 0;
    GLint	m_copyTransparentWindowShaderTex = -1;
    GLuint	m_copyShader = 0;
    GLint	m_copyShaderTex = -1;
    GLuint	m_copyVAO = 0;

    float	m_elapsed = 0.0f;
    float	m_animTime = 0.0f;
    std::unique_ptr<saba::VMDCameraAnimation>	m_vmdCameraAnim;

    bool Setup();
    void Clear();

    void SetupTransparentFBO();

    Texture GetTexture(const std::string& texturePath);
};
