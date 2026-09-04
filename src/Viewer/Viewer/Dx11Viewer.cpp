#include "Viewer/Viewer/Dx11Viewer.h"

#include "Viewer/Instance/Dx11Instance.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstring>
#include <utility>

namespace Chrivent {
	GraphicsError::Result<void> Dx11Viewer::CreateDummyResources() {
		const auto textureResult = textureCache.CreateWhiteTexture(device.GetDevice());
		if (!textureResult)
			return std::unexpected(textureResult.error());
		dummyTexture.texture = textureResult->texture;
		dummyTexture.textureView = textureResult->textureView;
		return {};
	}

	GraphicsError::Result<void> Dx11Viewer::SetupCore(const SceneShaderRuntimeContract& shaderContract) {
		BindPostProcess(postProcess);
		HWND__* hwnd = glfwGetWin32Window(window);
		const auto deviceResult = device.Initialize(capabilities);
		if (!deviceResult)
			return std::unexpected(deviceResult.error());
		device.SelectMsaaSettings(multiSampleCount, multiSampleQuality, capabilities);
		const auto swapChainResult = swapChain.Initialize(device.GetDevice(), hwnd);
		if (!swapChainResult)
			return std::unexpected(swapChainResult.error());
		const auto targetResult = renderTargets.Initialize(device.GetDevice(), swapChain.GetSwapChain(),
			screenWidth, screenHeight, multiSampleCount, multiSampleQuality);
		if (!targetResult)
			return std::unexpected(targetResult.error());
		const auto pipelineResult = pipeline.Initialize(device.GetDevice(), shaderContract);
		if (!pipelineResult)
			return std::unexpected(pipelineResult.error());
		const auto dummyResult = CreateDummyResources();
		if (!dummyResult)
			return std::unexpected(dummyResult.error());
		Dx11DrawContext::ApplyViewport(device.GetContext(), screenWidth, screenHeight);
		return {};
	}

	GraphicsError::Result<void> Dx11Viewer::ResizeCore() {
		if (frameCaptureWidth != static_cast<UINT>(screenWidth)
			|| frameCaptureHeight != static_cast<UINT>(screenHeight)) {
			frameCaptureStagingTexture.Reset();
			frameCapturePixels.clear();
			frameCaptureWidth = 0;
			frameCaptureHeight = 0;
		}
		renderTargets.Reset(device.GetContext());
		postProcess.ResetTargets();
		const auto resizeResult = swapChain.Resize(screenWidth, screenHeight);
		if (!resizeResult)
			return std::unexpected(resizeResult.error());
		const auto targetResult = renderTargets.Initialize(device.GetDevice(), swapChain.GetSwapChain(),
			screenWidth, screenHeight, multiSampleCount, multiSampleQuality);
		if (!targetResult)
			return std::unexpected(targetResult.error());
		if (postProcess.HasEffects()) {
			const auto postProcessResult = postProcess.InitializeTargets(
				device.GetDevice(), screenWidth, screenHeight);
			if (!postProcessResult)
				return std::unexpected(postProcessResult.error());
		}
		Dx11DrawContext::ApplyViewport(device.GetContext(), screenWidth, screenHeight);
		return {};
	}

	GraphicsError::Result<FrameBeginState> Dx11Viewer::BeginFrameCore() {
		drawContext.BeginFrame();
		ID3D11RenderTargetView* sceneColorView = renderTargets.GetSceneColorView();
		device.GetContext()->ClearRenderTargetView(sceneColorView, clearColor);
		device.GetContext()->ClearDepthStencilView(renderTargets.GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		device.GetContext()->OMSetRenderTargets(1, &sceneColorView,
			renderTargets.GetDepthStencilView());
		pipeline.BindDefaultBlendState(device.GetContext());
		return FrameBeginState::Ready;
	}

	GraphicsError::Result<FrameEndState> Dx11Viewer::EndFrameCore() {
		if (postProcess.HasEffects()) {
			const auto drawResult = postProcess.Draw(device.GetContext(),
				renderTargets.GetSceneColor(), multiSampleCount, renderTargets.GetBackBufferView(),
				screenWidth, screenHeight, GetPostProcessFrameData());
			if (!drawResult)
				return std::unexpected(drawResult.error());
		} else {
			if (multiSampleCount > 1)
				device.GetContext()->ResolveSubresource(renderTargets.GetBackBuffer(), 0,
					renderTargets.GetSceneColor(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
			else
				device.GetContext()->CopyResource(renderTargets.GetBackBuffer(),
					renderTargets.GetSceneColor());
		}
		if (pendingFrameCaptureSink) {
			FrameCaptureSink captureSink = std::move(pendingFrameCaptureSink);
			pendingFrameCaptureSink = {};
			const auto captureResult = ReadBackBuffer();
			if (!captureResult)
				return std::unexpected(captureResult.error());
			if (!captureSink(frameCapturePixels))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
					"video frame capture", "FFmpeg에 캡처 프레임을 전달하지 못했습니다"));
		}
		const auto presentResult = swapChain.Present();
		if (!presentResult)
			return std::unexpected(presentResult.error());
		return FrameEndState::Presented;
	}

	GraphicsError::Result<void> Dx11Viewer::ReadBackBuffer() {
		ID3D11Texture2D* backBuffer = renderTargets.GetBackBuffer();
		if (!backBuffer)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"video frame capture", "캡처할 back buffer가 없습니다"));
		D3D11_TEXTURE2D_DESC description{};
		backBuffer->GetDesc(&description);
		if (description.Format != DXGI_FORMAT_R8G8B8A8_UNORM)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::UnsupportedFeature,
				"video frame capture", "지원하지 않는 back buffer 형식입니다"));
		description.Usage = D3D11_USAGE_STAGING;
		description.BindFlags = 0;
		description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		description.MiscFlags = 0;
		description.SampleDesc.Count = 1;
		description.SampleDesc.Quality = 0;
		if (!frameCaptureStagingTexture || frameCaptureWidth != description.Width
			|| frameCaptureHeight != description.Height) {
			frameCaptureStagingTexture.Reset();
			const HRESULT createResult = device.GetDevice()->CreateTexture2D(
				&description, nullptr, frameCaptureStagingTexture.GetAddressOf());
			if (FAILED(createResult))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
					"video frame capture", "CPU readback texture를 만들지 못했습니다",
					static_cast<int64_t>(createResult), true));
			frameCaptureWidth = description.Width;
			frameCaptureHeight = description.Height;
			frameCapturePixels.resize(static_cast<size_t>(description.Width)
				* description.Height * 4);
		}
		device.GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
		device.GetContext()->CopyResource(frameCaptureStagingTexture.Get(), backBuffer);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT mapResult = device.GetContext()->Map(
			frameCaptureStagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(mapResult))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"video frame capture", "GPU 출력 이미지를 CPU에서 읽지 못했습니다",
				static_cast<int64_t>(mapResult), true));
		const size_t rowSize = static_cast<size_t>(description.Width) * 4;
		for (UINT row = 0; row < description.Height; row++) {
			const auto* source = static_cast<const uint8_t*>(mapped.pData)
				+ static_cast<size_t>(row) * mapped.RowPitch;
			std::memcpy(frameCapturePixels.data() + static_cast<size_t>(row) * rowSize, source, rowSize);
		}
		device.GetContext()->Unmap(frameCaptureStagingTexture.Get(), 0);
		return {};
	}

	GraphicsError::Result<void> Dx11Viewer::BeginPostProcessSceneInputPassCore() {
		return postProcess.BeginSceneInputPass(device.GetContext(),
			pipeline.GetDefaultDepthStencilState(), screenWidth, screenHeight);
	}

	GraphicsError::Result<void> Dx11Viewer::EndPostProcessSceneInputPassCore() {
		return postProcess.EndSceneInputPass(device.GetContext());
	}

	GraphicsError::Result<void> Dx11Viewer::WaitIdleCore() {
		return device.WaitIdle();
	}

	GraphicsError::Result<void> Dx11Viewer::LoadPostProcessEffectsCore(
		PreparedPostProcessEffects preparedEffects) {
		return postProcess.Configure(
			device.GetDevice(), screenWidth, screenHeight, std::move(preparedEffects));
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstanceCore() {
		return std::make_unique<Dx11Instance>(device, textureCache, drawContext);
	}
}
