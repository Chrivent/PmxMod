#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/PostProcess/Dx11PostProcess.h"
#include "Viewer/Texture/Dx11TextureCache.h"
#include "Viewer/Device/Dx11Device.h"
#include "Viewer/Pipeline/Dx11Pipeline.h"
#include "Viewer/RenderTarget/Dx11RenderTargets.h"
#include "Viewer/SwapChain/Dx11SwapChain.h"

#include <memory>
#include <vector>

namespace Chrivent {
	// 공통 Viewer 계약을 D3D11 렌더링 흐름으로 구현한다.
	class Dx11Viewer : public Viewer {
		UINT multiSampleCount = 4;
		UINT multiSampleQuality = 0;
		Dx11Device device;
		Dx11SwapChain swapChain;
		Dx11TextureCache textureCache;
		Dx11PostProcess postProcess;
		Dx11RenderTargets renderTargets;
		Dx11Pipeline pipeline;
		Dx11DummyTexture dummyTexture;
		Dx11DrawContext drawContext{ device, pipeline, dummyTexture };

		// 텍스처가 없는 재질에 사용할 기본 DX11 리소스를 생성한다.
		GraphicsError::Result<void> CreateDummyResources();

	protected:
		// 체크된 후처리 셰이더들을 DX11 ping-pong 체인으로 준비한다.
		GraphicsError::Result<void> LoadPostProcessEffectsCore(
			const std::vector<const EffectRuntimeDefinition*>& effects) override;
		// DX11 후처리 장면 입력 패스 기록을 시작한다.
		GraphicsError::Result<void> BeginPostProcessSceneInputPassCore() override;
		// DX11 후처리 장면 입력 패스를 종료한다.
		GraphicsError::Result<void> EndPostProcessSceneInputPassCore() override;
		// DX11 디바이스, 스왑체인과 파이프라인 리소스를 초기화한다.
		GraphicsError::Result<void> SetupCore(const SceneShaderRuntimeContract& shaderContract) override;
		// 창 크기에 맞춰 DX11 렌더 타깃과 깊이 버퍼를 재생성한다.
		GraphicsError::Result<void> ResizeCore() override;
		// 장면 색상과 깊이 타깃을 지우고 DX11 프레임을 시작한다.
		GraphicsError::Result<FrameBeginState> BeginFrameCore() override;
		// 장면 색상을 스왑체인으로 복사하고 표시 결과를 반환한다.
		GraphicsError::Result<FrameEndState> EndFrameCore() override;
		// DX11 immediate context에 제출된 명령이 끝날 때까지 기다린다.
		GraphicsError::Result<void> WaitIdleCore() override;
		// 초기 상태의 DX11 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstanceCore() override;

	public:
		Dx11Viewer() : Viewer(GraphicsApi::DirectX11, true) {}
	};
}
