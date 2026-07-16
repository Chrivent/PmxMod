#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/PostProcess/Dx11PostProcess.h"
#include "Viewer/Texture/Dx11TextureCache.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace Chrivent {
	// D3D11 Viewer가 소유하는 device, immediate context와 swapchain을 보관한다.
	struct Dx11DeviceResources {
		Microsoft::WRL::ComPtr<ID3D11Device> device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
		Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
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
		Dx11DrawContext drawContext{ deviceResources.device, deviceResources.context,
			shaders, pipelineStates, dummyTexture };

		// 지정한 sample count를 색상과 depth 타깃이 함께 지원하는지 확인한다.
		static bool ResolveMsaaQuality(ID3D11Device* device, UINT sampleCount, UINT& quality);
		// 현재 디바이스와 렌더 타깃 형식이 지원하는 최대 MSAA sample count를 반환한다.
		static UINT ResolveMaximumMsaaSampleCount(ID3D11Device* device);
		// 공통 4→2→1 정책으로 실제 사용할 MSAA sample count와 quality를 선택한다.
		void ChooseMsaaSettings();
        // DX11을 지원하는 고성능 DXGI 어댑터를 선택해 디바이스를 생성한다.
        bool CreateDevice();
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
        bool LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) override;
		// DX11 후처리 장면 입력 패스 기록을 시작한다.
		bool BeginPostProcessSceneInputPassCore() override;
        // 초기 상태의 DX11 모델 인스턴스를 생성한다.
        std::unique_ptr<Instance> CreateInstanceCore() override;
    
    public:
		const Dx11DrawContext& GetDrawContext() const { return drawContext; }

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
