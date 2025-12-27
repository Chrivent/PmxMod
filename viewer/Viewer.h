#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <glad/glad.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <map>
#include <filesystem>

struct SceneConfig;
struct MMDMaterial;
struct GLFWAppContext;
class MMDModel;
class VMDAnimation;
class VMDCameraAnimation;

GLuint CreateShader(GLenum shaderType, const std::string &code);
GLuint CreateShaderProgram(const std::filesystem::path& vsFile, const std::filesystem::path& fsFile);

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
    GLint	m_uLightVP = -1;
    GLint	m_uShadowMapSplitPositions = -1;
    GLint	m_uShadowMap0 = -1;
    GLint	m_uShadowMap1 = -1;
    GLint	m_uShadowMap2 = -1;
    GLint	m_uShadowMap3 = -1;
    GLint	m_uShadowMapEnabled = -1;

    bool Setup(const GLFWAppContext& appContext);
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

    bool Setup(const GLFWAppContext& appContext);
};

struct GLFWGroundShadowShader {
    ~GLFWGroundShadowShader();

    GLuint	m_prog = 0;
    GLint	m_inPos = -1;
    GLint	m_uWVP = -1;
    GLint	m_uShadowColor = -1;

    bool Setup(const GLFWAppContext& appContext);
};

struct DX11Vertex {
    glm::vec3	m_position;
    glm::vec3	m_normal;
    glm::vec2	m_uv;
};

struct DX11VertexShader {
    glm::mat4	m_wv;
    glm::mat4	m_wvp;
};

struct DX11PixelShader {
    float		m_alpha;
    glm::vec3	m_diffuse;
    glm::vec3	m_ambient;
    float		m_dummy1;
    glm::vec3	m_specular;
    float		m_specularPower;
    glm::vec3	m_lightColor;
    float		m_dummy2;
    glm::vec3	m_lightDir;
    float		m_dummy3;
    glm::vec4	m_texMulFactor;
    glm::vec4	m_texAddFactor;
    glm::vec4	m_toonTexMulFactor;
    glm::vec4	m_toonTexAddFactor;
    glm::vec4	m_sphereTexMulFactor;
    glm::vec4	m_sphereTexAddFactor;
    glm::ivec4	m_textureModes;
};

struct DX11EdgeVertexShader {
    glm::mat4	m_wv;
    glm::mat4	m_wvp;
    glm::vec2	m_screenSize;
    float		m_dummy[2];
};

struct DX11EdgeSizeVertexShader {
    float		m_edgeSize;
    float		m_dummy[3];
};

struct DX11EdgePixelShader {
    glm::vec4	m_edgeColor;
};

struct DX11GroundShadowVertexShader {
    glm::mat4	m_wvp;
};

struct DX11GroundShadowPixelShader {
    glm::vec4	m_shadowColor;
};

struct GLFWTexture {
    GLuint	m_texture;
    bool	m_hasAlpha;
};

struct DX11Texture {
    Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_textureView;
    bool								m_hasAlpha;
};

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
};

struct GLFWAppContext : AppContext {
    ~GLFWAppContext();

    std::unique_ptr<GLFWShader>				m_shader;
    std::unique_ptr<GLFWEdgeShader>			m_edgeShader;
    std::unique_ptr<GLFWGroundShadowShader>	m_groundShadowShader;
    std::map<std::filesystem::path, GLFWTexture>	m_textures;
    GLuint	m_dummyColorTex = 0;
    GLuint	m_dummyShadowDepthTex = 0;
    const int	m_msaaSamples = 4;

    bool Setup();
    GLFWTexture GetTexture(const std::filesystem::path& texturePath);
};

struct DX11AppContext : AppContext {
    UINT		m_multiSampleCount = 1;
    UINT		m_multiSampleQuality = 0;
    std::map<std::filesystem::path, DX11Texture>	m_textures;
    Microsoft::WRL::ComPtr<ID3D11Device>			m_device;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	m_renderTargetView;
    Microsoft::WRL::ComPtr <ID3D11DepthStencilView> m_depthStencilView;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_mmdVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_mmdPS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_mmdInputLayout;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>	m_textureSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>	m_toonTextureSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>	m_sphereTextureSampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState>	m_mmdBlendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	m_mmdFrontFaceRS;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	m_mmdBothFaceRS;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_mmdEdgeVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_mmdEdgePS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_mmdEdgeInputLayout;
    Microsoft::WRL::ComPtr<ID3D11BlendState>	m_mmdEdgeBlendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	m_mmdEdgeRS;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	m_mmdGroundShadowVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	m_mmdGroundShadowPS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	m_mmdGroundShadowInputLayout;
    Microsoft::WRL::ComPtr<ID3D11BlendState>	m_mmdGroundShadowBlendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	m_mmdGroundShadowRS;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>	m_mmdGroundShadowDSS;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>	m_defaultDSS;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_dummyTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_dummyTextureView;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>			m_dummySampler;

    bool Setup(const Microsoft::WRL::ComPtr<ID3D11Device>& device);
    bool CreateShaders();
    DX11Texture GetTexture(const std::filesystem::path& texturePath);

    static bool ReadCsoBinary(const std::filesystem::path& path, std::vector<uint8_t>& out);
};

struct GLFWMaterial {
    explicit GLFWMaterial(const MMDMaterial& mat);

    const MMDMaterial& m_mmdMat;
    GLuint	m_texture = 0;
    bool	m_textureHasAlpha = false;
    GLuint	m_spTexture = 0;
    GLuint	m_toonTexture = 0;
};

struct DX11Material {
    explicit DX11Material(const MMDMaterial& mat);

    const MMDMaterial&	m_mmdMat;
    DX11Texture	m_texture{};
    DX11Texture	m_spTexture{};
    DX11Texture	m_toonTexture{};
};

struct Model {
    std::shared_ptr<MMDModel>	m_mmdModel;
    std::unique_ptr<VMDAnimation>	m_vmdAnim;
    float m_scale = 1.0f;
};

struct GLFWModel : Model {
    GLuint	m_posVBO = 0;
    GLuint	m_norVBO = 0;
    GLuint	m_uvVBO = 0;
    GLuint	m_ibo = 0;
    GLenum	m_indexType;
    GLuint	m_mmdVAO = 0;
    GLuint	m_mmdEdgeVAO = 0;
    GLuint	m_mmdGroundShadowVAO = 0;
    std::vector<GLFWMaterial>	m_materials;

    bool Setup(GLFWAppContext& appContext);
    void Clear();
    void UpdateAnimation(const GLFWAppContext& appContext) const;
    void Update() const;
    void Draw(const GLFWAppContext& appContext) const;
};

struct DX11Model : Model {
    std::vector<DX11Material>	                m_materials;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>	m_context;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_indexBuffer;
    DXGI_FORMAT					                m_indexBufferFormat;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_mmdVSConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_mmdPSConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_mmdEdgeVSConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_mmdEdgeSizeVSConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_mmdEdgePSConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_mmdGroundShadowVSConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>		m_mmdGroundShadowPSConstantBuffer;

    bool Setup(DX11AppContext& appContext);
    void UpdateAnimation(const DX11AppContext& appContext) const;
    void Update() const;
    void Draw(const DX11AppContext& appContext) const;
};

bool GLFWSampleMain(const SceneConfig& cfg);
bool DX11SampleMain(const SceneConfig& cfg);
