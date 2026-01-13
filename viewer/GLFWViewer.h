#pragma once

#include "Viewer.h"

#include <map>

struct GLFWViewer;

struct GLFWShader {
    ~GLFWShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_inNor = -1;
    GLint	m_inUV = -1;
    GLint	m_uWV = -1;
    GLint	m_uWVP = -1;
    GLint	m_uAmbient = -1;
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

    bool Setup(const GLFWViewer& viewer);
};

struct GLFWEdgeShader {
    ~GLFWEdgeShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_inNor = -1;
    GLint	m_uWV = -1;
    GLint	m_uWVP = -1;
    GLint	m_uScreenSize = -1;
    GLint	m_uEdgeSize = -1;
    GLint	m_uEdgeColor = -1;

    bool Setup(const GLFWViewer& viewer);
};

struct GLFWGroundShadowShader {
    ~GLFWGroundShadowShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_uWVP = -1;
    GLint	m_uShadowColor = -1;

    bool Setup(const GLFWViewer& viewer);
};

struct GLFWTexture {
    GLuint	m_texture;
    bool	m_hasAlpha;
};

struct GLFWMaterial {
    explicit GLFWMaterial(const MMDMaterial& mat);

    const MMDMaterial& m_mmdMat;
    GLuint	m_texture = 0;
    bool	m_textureHasAlpha = false;
    GLuint	m_spTexture = 0;
    GLuint	m_toonTexture = 0;
};

struct GLFWModel : Model {
    GLFWViewer* m_viewer;
    GLuint	m_posVBO = 0;
    GLuint	m_norVBO = 0;
    GLuint	m_uvVBO = 0;
    GLuint	m_ibo = 0;
    GLenum	m_indexType;
    GLuint	m_mmdVAO = 0;
    GLuint	m_mmdEdgeVAO = 0;
    GLuint	m_mmdGroundShadowVAO = 0;
    std::vector<GLFWMaterial>	m_materials;

    bool Setup(Viewer& viewer) override;
    void Clear() override;
    void Update() const override;
    void Draw() const override;
};

struct GLFWViewer : Viewer {
    ~GLFWViewer() override;

    std::unique_ptr<GLFWShader>				m_shader;
    std::unique_ptr<GLFWEdgeShader>			m_edgeShader;
    std::unique_ptr<GLFWGroundShadowShader>	m_groundShadowShader;
    std::map<std::filesystem::path, GLFWTexture>	m_textures;
    GLuint	m_dummyColorTex = 0;
    const int	m_msaaSamples = 4;

    void ConfigureGlfwHints() override;
    bool Setup() override;
    bool Resize() override;
    void BeginFrame() override;
    bool EndFrame() override;
    std::unique_ptr<Model> CreateModel() const override;

    GLFWTexture GetTexture(const std::filesystem::path& texturePath, bool clamp = false);
};
