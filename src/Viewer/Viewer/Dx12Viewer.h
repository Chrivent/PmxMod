#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/DrawContext/Dx12DrawContext.h"
#include "Viewer/Texture/Dx12TextureCache.h"
#include "Viewer/PostProcess/Dx12PostProcess.h"
#include "Viewer/Command/Dx12CommandContext.h"
#include "Viewer/RenderTarget/Dx12DepthBuffer.h"
#include "Viewer/Device/Dx12Device.h"
#include "Viewer/RenderTarget/Dx12MsaaColorBuffer.h"
#include "Viewer/Pipeline/Dx12Pipeline.h"
#include "Viewer/SwapChain/Dx12SwapChain.h"

#include <memory>
#include <vector>

namespace Chrivent {
	// 공통 Viewer 계약을 D3D12 명령 목록과 스왑체인 흐름으로 구현한다.
	class Dx12Viewer : public Viewer {
		Dx12Device device;
		Dx12SwapChain swapChain;
		Dx12MsaaColorBuffer msaaColorBuffer;
		Dx12DepthBuffer depthBuffer;
		Dx12CommandContext commandContext;
		Dx12Pipeline pipeline;
		Dx12TextureCache textureCache;
		Dx12PostProcess postProcess;
		Dx12Texture dummyTexture;
		Dx12DrawContext drawContext{ commandContext, pipeline };

		// 현재 back buffer를 render target 상태로 전환한다.
		void PrepareBackBufferForRendering(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) const;
		// MSAA color/depth target을 바인딩하고 프레임 시작 clear를 수행한다.
		void ClearRenderTargets(ID3D12GraphicsCommandList* commandList) const;
		
	protected:
		// 체크된 후처리 효과들을 DX12 ping-pong 체인으로 컴파일한다.
		bool LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) override;
		// DX12 후처리 장면 입력 패스 기록을 시작한다.
		bool BeginPostProcessSceneInputPassCore() override;
		// DX12 후처리 장면 입력 패스를 종료한다.
		bool EndPostProcessSceneInputPassCore() override;
		// DX12 디바이스, 스왑체인과 파이프라인 리소스를 초기화한다.
		bool SetupCore() override;
		// 창 크기에 맞춰 DX12 스왑체인과 렌더 타깃을 재생성한다.
		bool ResizeCore() override;
		// DX12 프레임 명령 기록을 시작한다.
		FrameBeginResult BeginFrameCore() override;
		// DX12 command list 제출과 Present 결과를 반환한다.
		FrameEndResult EndFrameCore() override;
		// 초기 상태의 DX12 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstanceCore() override;

	public:
		// DX12 command queue에 제출된 작업이 끝날 때까지 기다린다.
		bool WaitIdle() override;
	};
}
