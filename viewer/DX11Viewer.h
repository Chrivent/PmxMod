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

    /// 모델 데이터를 DX11 버퍼와 재질 리소스로 업로드한다.
    bool Setup(Viewer& viewer) override;
    /// 모델의 갱신된 버텍스 데이터를 DX11 버퍼에 반영한다.
    void Update() const override;
    /// 일반 메시, 엣지, 그림자 패스를 DX11로 렌더링한다.
    void Draw() const override;
};

struct DX11Viewer : Viewer {
    UINT    m_multiSampleCount = 4;
    UINT	m_multiSampleQuality = 0;
    std::map<std::filesystem::path, DX11Texture>	    m_textures;
    Microsoft::WRL::ComPtr<ID3D11Device>			    m_device;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	    m_renderTargetView;
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

    /// DX11 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
    void ConfigureGlfwHints() override;
    /// DX11 디바이스, 스왑체인, 파이프라인 리소스를 초기화한다.
    bool Setup() override;
    /// 창 크기에 맞춰 DX11 렌더 타깃과 깊이 버퍼를 재생성한다.
    bool Resize() override;
    /// 렌더 타깃을 지우고 프레임 렌더링을 시작한다.
    void BeginFrame() override;
    /// 스왑체인을 present해 프레임을 화면에 표시한다.
    bool EndFrame() override;
    /// DX11 모델 인스턴스를 생성한다.
    std::unique_ptr<Instance> CreateInstance() const override;

    /// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX11 리소스로 반환한다.
    DX11Texture LoadTexture(const std::filesystem::path& texturePath);
    /// 현재 화면 크기에 맞춰 DX11 뷰포트를 설정한다.
    void UpdateViewport() const;
    /// HLSL 버텍스 셰이더를 컴파일하고 DX11 셰이더 객체를 생성한다.
    bool MakeVS(const std::filesystem::path& f, const char* entry,
    Microsoft::WRL::ComPtr<ID3D11VertexShader>& outVS, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob) const;
    /// HLSL 픽셀 셰이더를 컴파일하고 DX11 셰이더 객체를 생성한다.
    bool MakePS(const std::filesystem::path& f, const char* entry,
        Microsoft::WRL::ComPtr<ID3D11PixelShader>& outPS) const;
    /// 모델, 엣지, 그림자 렌더링용 셰이더를 생성한다.
    bool CreateShaders();
    /// 스왑체인 렌더 타깃과 깊이 스텐실 리소스를 생성한다.
    bool CreateRenderTargets();
    /// 래스터라이저, 블렌드, 샘플러, 깊이 상태를 생성한다.
    bool CreatePipelineStates();
    /// 텍스처가 없는 재질에 사용할 기본 DX11 리소스를 생성한다.
    bool CreateDummyResources();
};
