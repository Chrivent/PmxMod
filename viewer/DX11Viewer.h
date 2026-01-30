#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <map>

#include "Viewer.h"

struct DX11Viewer;

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
    bool								                m_hasAlpha;
};

struct DX11Material {
    const Material& m_mat;
    DX11Texture m_texture{};
    DX11Texture	m_spTexture{};
    DX11Texture	m_toonTexture{};

    explicit DX11Material(const Material& mat);
};

struct DX11Instance : Instance {
    DX11Viewer*                             m_viewer;
    std::vector<DX11Material>               m_materials;
    Microsoft::WRL::ComPtr<ID3D11Buffer>    m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	m_indexBuffer;
    DXGI_FORMAT                             m_indexBufferFormat;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	m_vsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	m_psConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	m_edgeVsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	m_edgeSizeVsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	m_edgePsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	m_gsVsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	m_gsPsConstantBuffer;

    bool Setup(Viewer& viewer) override;
    void Update() const override;
    void Draw() const override;
};

struct DX11Viewer : Viewer {
    UINT    m_multiSampleCount = 4;
    UINT	m_multiSampleQuality = 0;
    std::map<std::filesystem::path, DX11Texture>	    m_textures;
    Microsoft::WRL::ComPtr<ID3D11Device>			    m_device;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>			    m_backBuffer;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	    m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>			    m_msaaColorTex;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	    m_msaaRenderTargetView;
    Microsoft::WRL::ComPtr <ID3D11DepthStencilView>     m_depthStencilView;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	        m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	        m_ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	        m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>	        m_textureSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>	        m_toonTextureSampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState>	        m_blendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	    m_frontFaceRs;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	    m_bothFaceRs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	        m_edgeVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	        m_edgePs;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	        m_edgeInputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	    m_edgeRs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	        m_gsVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	        m_gsPs;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	        m_gsInputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	    m_gsRs;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>	    m_gsDss;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>	    m_defaultDss;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>			    m_dummyTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    m_dummyTextureView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>             m_depthTex;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>         m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>              m_swapChain;

    void ConfigureGlfwHints() override;
    bool Setup() override;
    bool Resize() override;
    void BeginFrame() override;
    bool EndFrame() override;
    std::unique_ptr<Instance> CreateInstance() const override;

    DX11Texture GetTexture(const std::filesystem::path& texturePath);
    void UpdateViewport() const;
    bool MakeVS(const std::filesystem::path& f, const char* entry,
    Microsoft::WRL::ComPtr<ID3D11VertexShader>& outVS, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob) const;
    bool MakePS(const std::filesystem::path& f, const char* entry,
        Microsoft::WRL::ComPtr<ID3D11PixelShader>& outPS) const;
    bool CreateShaders();
    bool CreateRenderTargets();
    bool CreatePipelineStates();
    bool CreateDummyResources();
};
