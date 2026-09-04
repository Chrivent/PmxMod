#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/DrawContext/Dx11DrawContext.h"
#include "Viewer/PostProcess/Dx11PostProcess.h"
#include "Viewer/Texture/Dx11TextureCache.h"
#include "Viewer/Device/Dx11Device.h"
#include "Viewer/Pipeline/Dx11Pipeline.h"
#include "Viewer/RenderTarget/Dx11RenderTargets.h"
#include "Viewer/SwapChain/Dx11SwapChain.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace Chrivent {
	// 공통 Viewer 계약을 D3D11 렌더링 흐름으로 구현한다.
	class Dx11Viewer final : public Viewer {
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
		using FrameCaptureSink = std::function<bool(std::span<const uint8_t>)>;
		FrameCaptureSink pendingFrameCaptureSink;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> frameCaptureStagingTexture;
		std::vector<uint8_t> frameCapturePixels;
		UINT frameCaptureWidth = 0;
		UINT frameCaptureHeight = 0;

		// 텍스처가 없는 재질에 사용할 기본 DX11 리소스를 생성한다.
		GraphicsError::Result<void> CreateDummyResources();
		// 합성이 끝난 swapchain back buffer를 재사용 가능한 CPU 버퍼로 읽어낸다.
		GraphicsError::Result<void> ReadBackBuffer();

	protected:
		// 체크된 후처리 셰이더들을 DX11 ping-pong 체인으로 준비한다.
		GraphicsError::Result<void> LoadPostProcessEffectsCore(
			PreparedPostProcessEffects preparedEffects) override;
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
		// 다음 EndFrame에서 합성된 RGBA 프레임을 지정한 sink로 전달한다.
		void RequestFrameCapture(FrameCaptureSink sink) {
			pendingFrameCaptureSink = std::move(sink);
		}
	};
}
