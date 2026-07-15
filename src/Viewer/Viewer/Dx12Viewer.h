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

#include <filesystem>
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
		bool frameReady = false;
		UINT frameIndex = 0;
		Dx12DrawContext drawContext{ commandContext, pipeline, frameReady, frameIndex };

		// 현재 back buffer를 render target 상태로 전환한다.
		void PrepareBackBufferForRendering(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) const;
		// MSAA color/depth target을 바인딩하고 프레임 시작 clear를 수행한다.
		void ClearRenderTargets(ID3D12GraphicsCommandList* commandList) const;
		// 현재 화면 크기에 맞는 viewport와 scissor rect를 명령 목록에 적용한다.
		void ApplyViewportAndScissor(ID3D12GraphicsCommandList* commandList) const;
		
	protected:
		PostProcess& ResolvePostProcess() override { return postProcess; }
		const PostProcess& ResolvePostProcess() const override { return postProcess; }
		
		// 체크된 후처리 효과들을 DX12 ping-pong 체인으로 컴파일한다.
		bool LoadPostProcessEffectsCore(const std::vector<const EffectDefinition*>& effects) override;
		// DX12 후처리 장면 입력 패스 기록을 시작한다.
		bool BeginPostProcessSceneInputPassCore() override;
		// 초기 상태의 DX12 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstanceCore() override;

	public:
		~Dx12Viewer() override;

		const Dx12Device& GetDevice() const { return device; }
		const Dx12Texture& GetDummyTexture() const { return dummyTexture; }
		UINT GetFrameIndex() const { return frameIndex; }
		const Dx12DrawContext& GetDrawContext() const { return drawContext; }
		
		// DX12 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
		void ConfigureWindowHints() override;
		// DX12 렌더러 리소스를 초기화한다.
		bool Setup() override;
		// 창 크기에 맞춰 DX12 스왑체인과 렌더 타깃을 재생성한다.
		bool Resize() override;
		// DX12 프레임 렌더링을 시작한다.
		FrameBeginResult BeginFrame() override;
		// DX12 프레임을 제출하고 화면에 표시한다.
		FrameEndResult EndFrame() override;
		// DX12 후처리 장면 입력 패스를 종료한다.
		bool EndPostProcessSceneInputPass() override;
		// DX12 command queue에 제출된 작업이 끝날 때까지 기다린다.
		bool WaitIdle() override;
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 DX12 텍스처로 반환한다.
		Dx12Texture LoadTexture(const std::filesystem::path& texturePath);
	};
}
