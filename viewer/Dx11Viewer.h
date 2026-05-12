#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Viewer.h"

class Dx11Viewer;

struct Dx11Vertex {
    glm::vec3	position;
    glm::vec3	normal;
    glm::vec2	uv;
};

struct Dx11VertexShader {
    glm::mat4	wv;
    glm::mat4	wvp;
};

struct Dx11PixelShader {
    float		alpha;
    glm::vec3	diffuse;
    glm::vec3	ambient;
    float		dummy1;
    glm::vec3	specular;
    float		specularPower;
    glm::vec3	lightColor;
    float		dummy2;
    glm::vec3	lightDir;
    float		dummy3;
    glm::vec4	texMulFactor;
    glm::vec4	texAddFactor;
    glm::vec4	cartoonTexMulFactor;
    glm::vec4	cartoonTexAddFactor;
    glm::vec4	sphereTexMulFactor;
    glm::vec4	sphereTexAddFactor;
    glm::ivec4	textureModes;
};

struct Dx11EdgeVertexShader {
    glm::mat4	wv;
    glm::mat4	wvp;
    glm::vec2	screenSize;
    float		dummy[2];
};

struct Dx11EdgeSizeVertexShader {
    float		edgeSize;
    float		dummy[3];
};

struct Dx11EdgePixelShader {
    glm::vec4	edgeColor;
};

struct Dx11GroundShadowVertexShader {
    glm::mat4	wvp;
};

struct Dx11GroundShadowPixelShader {
    glm::vec4	shadowColor;
};

struct Dx11Texture {
    Microsoft::WRL::ComPtr<ID3D11Texture2D>				texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	textureView;
    bool								                hasAlpha;
};

class Dx11Material {
public:
    const Material& mat;
    Dx11Texture texture{};
    Dx11Texture	spTexture{};
    Dx11Texture	cartoonTexture{};

    explicit Dx11Material(const Material& sourceMat);
};

class Dx11Instance : public Instance {
protected:
    // 일반 메시 패스를 DX11로 렌더링한다.
    void DrawModel() const override;
    // 엣지 패스를 DX11로 렌더링한다.
    void DrawEdge() const override;
    // 지면 그림자 패스를 DX11로 렌더링한다.
    void DrawGroundShadow() const override;
    // 모델 버텍스 데이터를 매 프레임 갱신할 동적 버퍼 설명자를 만든다.
    static D3D11_BUFFER_DESC MakeVertexBufferDesc(size_t vertexCount);
    // 모델 인덱스 데이터를 한 번 업로드할 immutable 버퍼 설명자를 만든다.
    static D3D11_BUFFER_DESC MakeIndexBufferDesc(size_t indexBytes);
    // 상수 버퍼 크기를 16바이트 정렬해 생성한다.
    template<typename T>
    static HRESULT CreateBuffer(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11Buffer>& out) {
        const UINT bytes = static_cast<UINT>(sizeof(T) + 15u & ~15u);
        const CD3D11_BUFFER_DESC desc(bytes, D3D11_BIND_CONSTANT_BUFFER);
        return device->CreateBuffer(&desc, nullptr, out.GetAddressOf());
    }
    // 텍스처 유무에 따라 실제 SRV 또는 더미 SRV를 픽셀 셰이더 슬롯에 바인딩한다.
    void BindTexture(UINT slot, const Dx11Texture& tex, ID3D11SamplerState* sampler,
        int modeIfPresent, int& outMode, glm::vec4& outMul, glm::vec4& outAdd,
        const glm::vec4& mulIn, const glm::vec4& addIn) const;
    // OpenGL 스타일 clip space를 DX11 depth range로 변환하는 행렬을 반환한다.
    static const glm::mat4& DxClipMatrix();

    Dx11Viewer*                             viewer = nullptr;
    std::vector<Dx11Material>               materials;
    Microsoft::WRL::ComPtr<ID3D11Buffer>    vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	indexBuffer;
    DXGI_FORMAT                             indexBufferFormat = DXGI_FORMAT_R8_UINT;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	vsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	psConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	edgeVsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	edgeSizeVsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	edgePsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	gsVsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>	gsPsConstantBuffer;
    
public:
    // 모델 데이터를 DX11 버퍼와 재질 리소스로 업로드한다.
    bool Setup(Viewer& baseViewer) override;
    // 모델의 갱신된 버텍스 데이터를 DX11 버퍼에 반영한다.
    void Update() const override;
};

class Dx11Viewer : public Viewer {
    // 지정한 필터와 주소 모드로 샘플러 설명자를 만든다.
    static D3D11_SAMPLER_DESC MakeSamplerDesc(D3D11_FILTER f, D3D11_TEXTURE_ADDRESS_MODE addr);
    // 컬링 모드와 front-face 방향으로 래스터라이저 설명자를 만든다.
    static D3D11_RASTERIZER_DESC MakeRasterizerDesc(D3D11_CULL_MODE cull, bool frontCcw);
    // 일반 알파 블렌딩용 블렌드 설명자를 만든다.
    static D3D11_BLEND_DESC MakeAlphaBlendDesc();
    // GLFW 윈도우와 MSAA 설정으로 스왑체인 설명자를 만든다.
    static DXGI_SWAP_CHAIN_DESC MakeSwapChainDesc(HWND__* hwnd, UINT sampleCount, UINT sampleQuality);
    // 2D 텍스처의 기본 설명자를 만든다.
    static D3D11_TEXTURE2D_DESC MakeTexture2DDesc(UINT width, UINT height, DXGI_FORMAT format,
        UINT bindFlags, UINT sampleCount = 1, UINT sampleQuality = 0);
    // 기본 깊이 테스트용 depth-stencil 설명자를 만든다.
    static D3D11_DEPTH_STENCIL_DESC MakeDefaultDepthStencilDesc();
    // 지면 그림자 스텐실 처리용 depth-stencil 설명자를 만든다.
    static D3D11_DEPTH_STENCIL_DESC MakeGroundShadowDepthStencilDesc();

    UINT    multiSampleCount = 4;
    UINT	multiSampleQuality = 0;
    std::map<std::filesystem::path, Dx11Texture>	    textures;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>	    renderTargetView;
    Microsoft::WRL::ComPtr <ID3D11DepthStencilView>     depthStencilView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>			    dummyTexture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>             depthTex;
    Microsoft::WRL::ComPtr<IDXGISwapChain>              swapChain;
    
public:
    Microsoft::WRL::ComPtr<ID3D11Device>			    device;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	        vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	        ps;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	        inputLayout;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>	        textureSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>	        cartoonTextureSampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState>	        blendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	    frontFaceRs;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	    bothFaceRs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	        edgeVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	        edgePs;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	        edgeInputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	    edgeRs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>	        gsVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>	        gsPs;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>	        gsInputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>	    gsRs;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>	    gsDss;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>	    defaultDss;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    dummyTextureView;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>         context;

    // DX11 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
    void ConfigureGlfwHints() override;
    // DX11 디바이스, 스왑체인, 파이프라인 리소스를 초기화한다.
    bool Setup() override;
    // 창 크기에 맞춰 DX11 렌더 타깃과 깊이 버퍼를 재생성한다.
    bool Resize() override;
    // 렌더 타깃을 지우고 프레임 렌더링을 시작한다.
    void BeginFrame() override;
    // 스왑체인을 present해 프레임을 화면에 표시한다.
    bool EndFrame() override;
    // DX11 모델 인스턴스를 생성한다.
    std::unique_ptr<Instance> CreateInstance() const override;
    // 텍스처를 캐시에서 찾거나 파일에서 로드해 DX11 리소스로 반환한다.
    Dx11Texture LoadTexture(const std::filesystem::path& texturePath);
    // 현재 화면 크기에 맞춰 DX11 뷰포트를 설정한다.
    void UpdateViewport() const;
    // HLSL 버텍스 셰이더를 컴파일하고 DX11 셰이더 객체를 생성한다.
    bool MakeVs(const std::filesystem::path& f, const char* entry,
    Microsoft::WRL::ComPtr<ID3D11VertexShader>& outVs, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob) const;
    // HLSL 픽셀 셰이더를 컴파일하고 DX11 셰이더 객체를 생성한다.
    bool MakePs(const std::filesystem::path& f, const char* entry,
        Microsoft::WRL::ComPtr<ID3D11PixelShader>& outPs) const;
    // 모델, 엣지, 그림자 렌더링용 셰이더를 생성한다.
    bool CreateShaders();
    // 스왑체인 렌더 타깃과 깊이 스텐실 리소스를 생성한다.
    bool CreateRenderTargets();
    // 래스터라이저, 블렌드, 샘플러, 깊이 상태를 생성한다.
    bool CreatePipelineStates();
    // 텍스처가 없는 재질에 사용할 기본 DX11 리소스를 생성한다.
    bool CreateDummyResources();
};
