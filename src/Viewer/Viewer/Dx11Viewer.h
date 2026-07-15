#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/PostProcess/Dx11PostProcess.h"
#include "Viewer/Texture/Dx11TextureCache.h"
#include "Viewer/Shader/Dx11Shader.h"

#include <filesystem>
#include <memory>
#include <vector>
#include <dxgi1_6.h>

namespace Chrivent {
    // D3D11 device, immediate context와 swapchain을 한 단위로 보관한다.
    struct Dx11DeviceResources {
        Microsoft::WRL::ComPtr<ID3D11Device>        device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        Microsoft::WRL::ComPtr<IDXGISwapChain>      swapChain;
    };

    // D3D11 장면 렌더링에 사용하는 색상 및 depth target들을 보관한다.
    struct Dx11RenderTargets {
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         backBuffer;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  backBufferView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         sceneColorMsaa;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  sceneColorMsaaView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  depthStencilView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D>         depthTex;
    };

    // D3D11 기본 렌더링 패스별 셰이더 프로그램을 보관한다.
    struct Dx11ShaderSet {
        Dx11ModelShader         model;
        Dx11EdgeShader          edge;
        Dx11GroundShadowShader  groundShadow;
		Dx11SceneDepthShader sceneDepth;
		Dx11SceneVelocityShader sceneVelocity;
    };

    // D3D11 기본 렌더링에 사용하는 sampler와 고정 파이프라인 상태를 보관한다.
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

    // texture가 없는 D3D11 재질에 바인딩할 기본 흰색 texture를 보관한다.
    struct Dx11DummyTexture {
        Microsoft::WRL::ComPtr<ID3D11Texture2D>             texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>    textureView;
    };

    // 공통 Viewer 계약을 D3D11 렌더링 흐름으로 구현한다.
    class Dx11Viewer : public Viewer {
        UINT                multiSampleCount = 4;
        UINT	            multiSampleQuality = 0;
        Dx11TextureCache    textureCache;
        Dx11PostProcess postProcess;
        Dx11DeviceResources deviceResources;
        Dx11RenderTargets renderTargets;
        Dx11ShaderSet shaders;
        Dx11PipelineStates pipelineStates;
        Dx11DummyTexture dummyTexture;

        // DX11을 지원하는 고성능 DXGI 어댑터를 선택해 디바이스를 생성한다.
        bool CreateDevice();
        // 현재 화면 크기에 맞춰 DX11 뷰포트를 설정한다.
        void UpdateViewport() const;
        // 모델, 엣지, 그림자 렌더링용 셰이더를 생성한다.
        bool CreateShaders();
        // 스왑체인과 장면 색상, 깊이 스텐실 리소스를 생성한다.
        bool CreateRenderTargets();
        // 래스터라이저, 블렌드, 샘플러, 깊이 상태를 생성한다.
        bool CreatePipelineStates();
        // 텍스처가 없는 재질에 사용할 기본 DX11 리소스를 생성한다.
        bool CreateDummyResources();

    protected:
        PostProcess& ResolvePostProcess() override { return postProcess; }
        const PostProcess& ResolvePostProcess() const override { return postProcess; }
        
        // 체크된 후처리 셰이더들을 DX11 ping-pong 체인으로 준비한다.
        bool LoadPostProcessEffectsCore(const std::vector<const EffectDefinition*>& effects) override;
		// DX11 후처리 장면 입력 패스 기록을 시작한다.
		bool BeginPostProcessSceneInputPassCore() override;
        // 초기 상태의 DX11 모델 인스턴스를 생성한다.
        std::unique_ptr<Instance> CreateInstanceCore() override;
    
    public:
        const Dx11DeviceResources& GetDeviceResources() const { return deviceResources; }
        const Dx11ShaderSet& GetShaders() const { return shaders; }
        const Dx11PipelineStates& GetPipelineStates() const { return pipelineStates; }
        const Dx11DummyTexture& GetDummyTexture() const { return dummyTexture; }

        // DX11 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
        void ConfigureWindowHints() override;
        // DX11 디바이스, 스왑체인, 파이프라인 리소스를 초기화한다.
        bool Setup() override;
        // 창 크기에 맞춰 DX11 렌더 타깃과 깊이 버퍼를 재생성한다.
        bool Resize() override;
        // 장면 색상과 깊이 타깃을 지우고 프레임 렌더링을 시작한다.
        FrameBeginResult BeginFrame() override;
        // 장면 색상을 스왑체인으로 복사하고 화면에 표시한다.
        FrameEndResult EndFrame() override;
        // DX11 후처리 장면 입력 패스를 종료한다.
        bool EndPostProcessSceneInputPass() override;
        // DX11 immediate context에 제출된 명령이 끝날 때까지 기다린다.
        bool WaitIdle() override;
        // 텍스처를 캐시에서 찾거나 파일에서 로드해 DX11 리소스로 반환한다.
        Dx11Texture LoadTexture(const std::filesystem::path& texturePath) {
            return textureCache.Load(deviceResources.device.Get(), texturePath);
        }
    };
}
