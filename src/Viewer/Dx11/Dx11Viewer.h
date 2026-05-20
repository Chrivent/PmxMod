#pragma once

#include "../Viewer.h"
#include "../ViewerMaterial.h"
#include "Dx11TextureCache.h"

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

    struct Dx11Material : ViewerMaterial {
        Dx11Texture texture{};
        Dx11Texture	spTexture{};
        Dx11Texture	cartoonTexture{};

        explicit Dx11Material(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
    };

    struct Dx11DeviceResources {
        Microsoft::WRL::ComPtr<ID3D11Device>        device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        Microsoft::WRL::ComPtr<IDXGISwapChain>      swapChain;
    };

    struct Dx11RenderTargets {
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  depthStencilView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         depthTex;
    };

    struct Dx11ShaderSet {
        Microsoft::WRL::ComPtr<ID3D11VertexShader>  vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>   ps;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>  edgeVs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>   edgePs;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>   edgeInputLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>  gsVs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>   gsPs;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>   gsInputLayout;
    };

    struct Dx11PipelineStates {
        Microsoft::WRL::ComPtr<ID3D11SamplerState>      textureSampler;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>      cartoonTextureSampler;
        Microsoft::WRL::ComPtr<ID3D11BlendState>        blendState;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   frontFaceRs;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   bothFaceRs;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   edgeRs;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   gsRs;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> gsDss;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> defaultDss;
    };

    struct Dx11DummyTexture {
        Microsoft::WRL::ComPtr<ID3D11Texture2D>             texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    textureView;
    };

    class Dx11Viewer : public Viewer {
        // HLSL 컴파일 실패 정보를 콘솔에 출력한다.
        static void PrintShaderCompileError(const std::filesystem::path& file, const char* entry, const char* target, ID3DBlob* errorBlob);

        UINT    multiSampleCount = 4;
        UINT	multiSampleQuality = 0;
        Dx11TextureCache textureCache;
    
    public:
        Dx11DeviceResources deviceResources;
        Dx11RenderTargets renderTargets;
        Dx11ShaderSet shaders;
        Dx11PipelineStates pipelineStates;
        Dx11DummyTexture dummyTexture;

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

