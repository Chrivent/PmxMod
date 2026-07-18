#include "Viewer/Viewer/Dx11Viewer.h"

#include "Viewer/Instance/Dx11Instance.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	bool Dx11Viewer::CreateDummyResources() {
		const Dx11Texture texture = textureCache.CreateWhiteTexture(device.GetDevice());
		dummyTexture.texture = texture.texture;
		dummyTexture.textureView = texture.textureView;
		return dummyTexture.texture && dummyTexture.textureView;
	}

	GraphicsResult<void> Dx11Viewer::SetupCore(const SceneShaderRuntimeContract& shaderContract) {
		BindPostProcess(postProcess);
		HWND__* hwnd = glfwGetWin32Window(window);
		const auto deviceResult = device.Initialize(capabilities);
		if (!deviceResult)
			return std::unexpected(deviceResult.error());
		device.SelectMsaaSettings(multiSampleCount, multiSampleQuality, capabilities);
		const auto swapChainResult = device.CreateSwapChain(hwnd);
		if (!swapChainResult)
			return std::unexpected(swapChainResult.error());
		if (!renderTargets.Initialize(device.GetDevice(), device.GetSwapChain(),
			screenWidth, screenHeight, multiSampleCount, multiSampleQuality))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"render target 초기화", "DirectX 11 render target을 만들지 못했습니다"));
		const auto pipelineResult = pipeline.Initialize(device.GetDevice(), shaderContract);
		if (!pipelineResult)
			return std::unexpected(pipelineResult.error());
		if (!CreateDummyResources())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"dummy texture 생성", "fallback texture를 만들지 못했습니다"));
		Dx11DrawContext::ApplyViewport(device.GetContext(), screenWidth, screenHeight);
		return {};
	}

	GraphicsResult<void> Dx11Viewer::ResizeCore() {
		renderTargets.Reset(device.GetContext());
		postProcess.ResetTargets();
		const HRESULT resizeResult = device.GetSwapChain()->ResizeBuffers(0, screenWidth,
			screenHeight, DXGI_FORMAT_UNKNOWN, 0);
		if (FAILED(resizeResult))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"swap chain 크기 변경", "DirectX 11 buffer 크기를 바꾸지 못했습니다", resizeResult, true));
		if (!renderTargets.Initialize(device.GetDevice(), device.GetSwapChain(),
			screenWidth, screenHeight, multiSampleCount, multiSampleQuality))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"render target 크기 변경", "DirectX 11 render target을 다시 만들지 못했습니다"));
		if (postProcess.HasEffects() && !postProcess.InitializeTargets(device.GetDevice(),
			device.GetContext(), screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"후처리 target 크기 변경", "DirectX 11 후처리 target을 다시 만들지 못했습니다"));
		Dx11DrawContext::ApplyViewport(device.GetContext(), screenWidth, screenHeight);
		return {};
	}

	GraphicsResult<FrameBeginState> Dx11Viewer::BeginFrameCore() {
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

	GraphicsResult<FrameEndState> Dx11Viewer::EndFrameCore() {
		if (postProcess.HasEffects()) {
			postProcess.ResolveSceneColor(
				device.GetContext(), renderTargets.GetSceneColor(), multiSampleCount);
			if (!postProcess.Draw(device.GetContext(), renderTargets.GetBackBufferView(),
				pipeline.GetPostProcessRasterizerState(), pipeline.GetToonTextureSampler(),
				screenWidth, screenHeight, GetPostProcessFrameData())) {
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
					"후처리 효과 draw", "DirectX 11 후처리 chain 실행에 실패했습니다"));
			}
		} else {
			if (multiSampleCount > 1)
				device.GetContext()->ResolveSubresource(renderTargets.GetBackBuffer(), 0,
					renderTargets.GetSceneColor(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
			else
				device.GetContext()->CopyResource(renderTargets.GetBackBuffer(),
					renderTargets.GetSceneColor());
		}
		const HRESULT presentResult = device.GetSwapChain()->Present(0, 0);
		if (FAILED(presentResult))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::PresentationFailed,
				"swap chain present", "DirectX 11 프레임을 표시하지 못했습니다", presentResult, true));
		return FrameEndState::Presented;
	}

	GraphicsResult<void> Dx11Viewer::BeginPostProcessSceneInputPassCore() {
		if (!postProcess.BeginSceneInputPass(device.GetContext(),
			pipeline.GetDefaultDepthStencilState(), screenWidth, screenHeight)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"후처리 장면 입력 패스 시작", "DirectX 11 장면 입력 패스를 시작하지 못했습니다"));
		}
		return {};
	}

	GraphicsResult<void> Dx11Viewer::EndPostProcessSceneInputPassCore() {
		if (device.GetContext() == nullptr)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 장면 입력 패스 종료", "DirectX 11 context를 사용할 수 없습니다"));
		postProcess.EndSceneInputPass(device.GetContext());
		return {};
	}

	GraphicsResult<void> Dx11Viewer::WaitIdle() {
		return device.WaitIdle();
	}

	GraphicsResult<void> Dx11Viewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (!postProcess.Configure(device.GetDevice(), device.GetContext(), screenWidth, screenHeight, effects))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::EffectConfigurationFailed,
				"후처리 효과 구성", "DirectX 11 효과 chain을 만들지 못했습니다"));
		return {};
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstanceCore() {
		return std::make_unique<Dx11Instance>(device, textureCache, drawContext);
	}
}
