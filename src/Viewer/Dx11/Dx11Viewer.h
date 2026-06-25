#pragma once

#include "Viewer/Viewer.h"
#include "Viewer/Dx11/Dx11TextureCache.h"
#include "Viewer/Dx11/Helper/Dx11Shader.h"

#include <filesystem>
#include <memory>
#include <vector>
#include <dxgi1_6.h>

namespace Chrivent {
    class Dx11Viewer;

    struct Dx11Material : ViewerMaterial {
        Dx11Texture texture{};
        Dx11Texture	sphereTexture{};
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
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         pingPongColor[2];
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  pingPongColorView[2];
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pingPongColorResourceView[2];
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  depthStencilView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         depthTex;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         postProcessDepth;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  postProcessDepthStencilView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> postProcessDepthView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         focusHistory[2];
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  focusHistoryView[2];
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> focusHistoryResourceView[2];
    };

    struct Dx11ShaderSet {
        Dx11ModelShader         model;
        Dx11EdgeShader          edge;
        Dx11GroundShadowShader  groundShadow;
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
        // 현재 화면 크기에 맞춰 DX11 뷰포트를 설정한다.
        void UpdateViewport() const;
        // 모델, 엣지, 그림자 렌더링용 셰이더를 생성한다.
        bool CreateShaders();
        // 스왑체인과 장면 색상, 깊이 스텐실 리소스를 생성한다.
        bool CreateRenderTargets();
        // 장면 색상을 셰이더 입력용 단일 샘플 텍스처로 변환한다.
        void ResolveSceneColor() const;
        // DOF용 자동 초점 히스토리 텍스처를 갱신한다.
        void UpdateFocusHistory();
        // 준비된 포스트 프로세스 셰이더로 장면 색상을 스왑체인에 그린다.
        void DrawPostProcess();
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
        Dx11PostProcessShader focusHistoryShader;
        std::vector<Dx11PostProcessShader> postProcessShaders;
        int focusHistoryIndex = 0;
        bool focusHistoryEnabled = false;

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
        // DX11 포스트 프로세스용 단일 샘플 depth-only 패스를 시작한다.
        bool BeginPostProcessDepthPass() override;
        // DX11 포스트 프로세스용 단일 샘플 depth-only 패스를 종료한다.
        void EndPostProcessDepthPass() override;
        // DX11 immediate context에 제출된 명령이 끝날 때까지 기다린다.
        void WaitIdle() override;
        // 체크된 후처리 셰이더들을 DX11 ping-pong 체인으로 준비한다.
        bool LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) override;
        // DX11 후처리 셰이더들을 해제한다.
        void ClearPostProcessEffects() override;
        // DX11 모델 인스턴스를 생성한다.
        std::unique_ptr<Instance> CreateInstance() const override;
        // 텍스처를 캐시에서 찾거나 파일에서 로드해 DX11 리소스로 반환한다.
        Dx11Texture LoadTexture(const std::filesystem::path& texturePath) {
            return textureCache.Load(deviceResources.device.Get(), texturePath);
        }
    };
}
