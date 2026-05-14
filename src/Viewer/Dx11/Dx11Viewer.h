#pragma once

#include "../Viewer.h"

#include <map>
#include <d3d11.h>
#include <wrl/client.h>

namespace Chrivent {
    struct Material;
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

    struct Dx11Material {
        const Material& mat;
        Dx11Texture texture{};
        Dx11Texture	spTexture{};
        Dx11Texture	cartoonTexture{};

        explicit Dx11Material(const Material& sourceMat) : mat(sourceMat) {}
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
}
