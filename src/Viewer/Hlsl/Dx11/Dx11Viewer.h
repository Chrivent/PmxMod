#pragma once

#include "../../Viewer.h"
#include "Dx11TextureCache.h"
#include "Helper/Dx11Shader.h"

#include <filesystem>
#include <memory>
#include <dxgi1_6.h>

namespace Chrivent {
    class Dx11Viewer;

    struct Dx11Vertex {
        glm::vec3	position;
        glm::vec3	normal;
        glm::vec2	uv;
    };

    struct Dx11Material : ViewerMaterial {
        Dx11Texture texture{};
        Dx11Texture	spTexture{};
        Dx11Texture	toonTexture{};

        explicit Dx11Material(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
    };

    struct Dx11DeviceResources {
        Microsoft::WRL::ComPtr<ID3D11Device>        device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        Microsoft::WRL::ComPtr<IDXGISwapChain>      swapChain;
    };

    struct Dx11RenderTargets {
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         backBuffer;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  backBufferView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         sceneColorMsaa;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  sceneColorMsaaView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         sceneColor;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneColorView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  depthStencilView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         depthTex;
    };

    struct Dx11ShaderSet {
        Dx11ModelShader         model;
        Dx11EdgeShader          edge;
        Dx11GroundShadowShader  groundShadow;
        Dx11PostProcessShader   postProcess;
    };

    struct Dx11PipelineStates {
        Microsoft::WRL::ComPtr<ID3D11SamplerState>      textureSampler;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>      toonTextureSampler;
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
        UINT                multiSampleCount = 4;
        UINT	            multiSampleQuality = 0;
        Dx11TextureCache    textureCache;

        // DX11을 지원하는 고성능 DXGI 어댑터를 선택해 디바이스를 생성한다.
        bool CreateDevice();
        // 선택된 DXGI 어댑터 정보를 공통 GPU 로그 형식으로 출력한다.
        static void PrintGpuInfo(const DXGI_ADAPTER_DESC1& description);
        // 현재 화면 크기에 맞춰 DX11 뷰포트를 설정한다.
        void UpdateViewport() const;
        // 모델, 엣지, 그림자 렌더링용 셰이더를 생성한다.
        bool CreateShaders();
        // 스왑체인과 장면 색상, 깊이 스텐실 리소스를 생성한다.
        bool CreateRenderTargets();
        // 장면 색상을 셰이더 입력용 단일 샘플 텍스처로 변환한다.
        void ResolveSceneColor() const;
        // 준비된 포스트 프로세스 셰이더로 장면 색상을 스왑체인에 그린다.
        void DrawPostProcess() const;
        // 래스터라이저, 블렌드, 샘플러, 깊이 상태를 생성한다.
        bool CreatePipelineStates();
        // 텍스처가 없는 재질에 사용할 기본 DX11 리소스를 생성한다.
        bool CreateDummyResources();
    
    public:
        Dx11DeviceResources deviceResources;
        Dx11RenderTargets renderTargets;
        Dx11ShaderSet shaders;
        Dx11PipelineStates pipelineStates;
        Dx11DummyTexture dummyTexture;

        Dx11Viewer() = default;

        // DX11 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
        void ConfigureGlfwHints() override;
        // DX11 디바이스, 스왑체인, 파이프라인 리소스를 초기화한다.
        bool Setup() override;
        // 창 크기에 맞춰 DX11 렌더 타깃과 깊이 버퍼를 재생성한다.
        bool Resize() override;
        // 장면 색상과 깊이 타깃을 지우고 프레임 렌더링을 시작한다.
        void BeginFrame() override;
        // 장면 색상을 스왑체인으로 복사하고 화면에 표시한다.
        bool EndFrame() override;
        // DX11 immediate context에 제출된 명령이 끝날 때까지 기다린다.
        void WaitIdle() override;
        // 첫 번째 패스를 DX11 풀스크린 포스트 프로세스 셰이더로 준비한다.
        bool LoadPostProcessEffect(const EffectDefinition& effect) override;
        // DX11 모델 인스턴스를 생성한다.
        std::unique_ptr<Instance> CreateInstance() const override;
        // 텍스처를 캐시에서 찾거나 파일에서 로드해 DX11 리소스로 반환한다.
        Dx11Texture LoadTexture(const std::filesystem::path& texturePath) {
            return textureCache.Load(deviceResources.device.Get(), texturePath);
        }
    };
}
