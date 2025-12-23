#pragma once

#include <map>
#include <filesystem>
#include <glad/glad.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

struct SceneConfig;
struct MMDMaterial;
struct AppContext;
class MMDModel;
class VMDAnimation;
class VMDCameraAnimation;

GLuint CreateShader(GLenum shaderType, const std::string &code);
GLuint CreateShaderProgram(const std::filesystem::path& vsFile, const std::filesystem::path& fsFile);

struct MMDShader {
    ~MMDShader();

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

struct MMDEdgeShader {
    ~MMDEdgeShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_inNor = -1;
    GLint	m_uWV = -1;
    GLint	m_uWVP = -1;
    GLint	m_uScreenSize = -1;
    GLint	m_uEdgeSize = -1;
    GLint	m_uEdgeColor = -1;

    bool Setup(const AppContext& appContext);
    void Clear();
};

struct MMDGroundShadowShader {
    ~MMDGroundShadowShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_uWVP = -1;
    GLint	m_uShadowColor = -1;

    bool Setup(const AppContext& appContext);
    void Clear();
};

struct Texture {
    GLuint	m_texture;
    bool	m_hasAlpha;
};

struct AppContext {
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

struct Material {
    explicit Material(const MMDMaterial& mat);

    const MMDMaterial& m_mmdMat;
    GLuint	m_texture = 0;
    bool	m_textureHasAlpha = false;
    GLuint	m_spTexture = 0;
    GLuint	m_toonTexture = 0;
};

struct Model {
    std::shared_ptr<MMDModel>	m_mmdModel;
    std::unique_ptr<VMDAnimation>	m_vmdAnim;
    GLuint	m_posVBO = 0;
    GLuint	m_norVBO = 0;
    GLuint	m_uvVBO = 0;
    GLuint	m_ibo = 0;
    GLenum	m_indexType;
    GLuint	m_mmdVAO = 0;
    GLuint	m_mmdEdgeVAO = 0;
    GLuint	m_mmdGroundShadowVAO = 0;
    std::vector<Material>	m_materials;
    float m_scale = 1.0f;

    bool Setup(AppContext& appContext);
    void Clear();
    void UpdateAnimation(const AppContext& appContext) const;
    void Update() const;
    void Draw(const AppContext& appContext) const;
};

bool SampleMain(const SceneConfig& cfg);
