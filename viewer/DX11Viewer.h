#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <map>

#include "Viewer.h"

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

struct DX11Texture {
    Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_textureView;
    bool								m_hasAlpha;
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

    bool Run(const SceneConfig& cfg) override;
    std::unique_ptr<Model> CreateModel() const override;
    static bool ReadCsoBinary(const std::filesystem::path& path, std::vector<uint8_t>& out);
    bool Setup(const Microsoft::WRL::ComPtr<ID3D11Device>& device);
    DX11Texture GetTexture(const std::filesystem::path& texturePath);
    bool CreateShaders();
};

struct DX11Material {
    explicit DX11Material(const MMDMaterial& mat);

    const MMDMaterial&	m_mmdMat;
    DX11Texture	m_texture{};
    DX11Texture	m_spTexture{};
    DX11Texture	m_toonTexture{};
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

    bool Setup(AppContext& appContext) override;
    void Update() const override;
    void Draw(AppContext& appContext) const override;
};
