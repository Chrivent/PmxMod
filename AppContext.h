#pragma once

#include <glad/glad.h>

#include <map>

#include "VMDAnimation.h"
#include "VMDCameraAnimation.h"

GLuint CreateShader(GLenum shaderType, const std::string code);
GLuint CreateShaderProgram(const std::string vsFile, const std::string fsFile);

struct AppContext;

struct Input
{
	std::string					m_modelPath;
	std::vector<std::string>	m_vmdPaths;
};

struct MMDShader
{
	~MMDShader()
	{
		Clear();
	}

	GLuint	m_prog = 0;

	// attribute
	GLint	m_inPos = -1;
	GLint	m_inNor = -1;
	GLint	m_inUV = -1;

	// uniform
	GLint	m_uWV = -1;
	GLint	m_uWVP = -1;

	GLint	m_uAmbinet = -1;
	GLint	m_uDiffuse = -1;
	GLint	m_uSpecular = -1;
	GLint	m_uSpecularPower = -1;
	GLint	m_uAlpha = -1;

	GLint	m_uTexMode = -1;
	GLint	m_uTex = -1;
	GLint	m_uTexMulFactor = -1;
	GLint	m_uTexAddFactor = -1;

	GLint	m_uSphereTexMode = -1;
	GLint	m_uSphereTex = -1;
	GLint	m_uSphereTexMulFactor = -1;
	GLint	m_uSphereTexAddFactor = -1;

	GLint	m_uToonTexMode = -1;
	GLint	m_uToonTex = -1;
	GLint	m_uToonTexMulFactor = -1;
	GLint	m_uToonTexAddFactor = -1;

	GLint	m_uLightColor = -1;
	GLint	m_uLightDir = -1;

	GLint	m_uLightVP = -1;
	GLint	m_uShadowMapSplitPositions = -1;
	GLint	m_uShadowMap0 = -1;
	GLint	m_uShadowMap1 = -1;
	GLint	m_uShadowMap2 = -1;
	GLint	m_uShadowMap3 = -1;
	GLint	m_uShadowMapEnabled = -1;

	bool Setup(const AppContext& appContext);
	void Clear();
};

struct MMDEdgeShader
{
	~MMDEdgeShader()
	{
		Clear();
	}

	GLuint	m_prog = 0;

	// attribute
	GLint	m_inPos = -1;
	GLint	m_inNor = -1;

	// uniform
	GLint	m_uWV = -1;
	GLint	m_uWVP = -1;
	GLint	m_uScreenSize = -1;
	GLint	m_uEdgeSize = -1;

	GLint	m_uEdgeColor = -1;

	bool Setup(const AppContext& appContext);
	void Clear();
};

struct MMDGroundShadowShader
{
	~MMDGroundShadowShader()
	{
		Clear();
	}

	GLuint	m_prog = 0;

	// attribute
	GLint	m_inPos = -1;

	// uniform
	GLint	m_uWVP = -1;
	GLint	m_uShadowColor = -1;

	bool Setup(const AppContext& appContext);
	void Clear();
};

struct Texture
{
	GLuint	m_texture;
	bool	m_hasAlpha;
};

struct AppContext
{
    ~AppContext()
    {
        Clear();
    }

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
