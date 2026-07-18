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
		if (!device.Initialize(capabilities))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InitializationFailed,
				"initialize device", "the DirectX 11 device could not be created"));
		device.SelectMsaaSettings(multiSampleCount, multiSampleQuality, capabilities);
		capabilities.Print();
		if (!device.CreateSwapChain(hwnd, multiSampleCount, multiSampleQuality))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"create swap chain", "the DirectX 11 swap chain could not be created"));
		if (!renderTargets.Initialize(device.GetDevice(), device.GetSwapChain(),
			screenWidth, screenHeight, multiSampleCount, multiSampleQuality))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize render targets", "the DirectX 11 render targets could not be created"));
		if (postProcess.HasEffects() && !postProcess.InitializeTargets(device.GetDevice(),
			device.GetContext(), screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize post-process targets", "the DirectX 11 post-process targets could not be created"));
		if (!pipeline.Initialize(device.GetDevice(), shaderContract.builtIn, shaderContract.sceneInput))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize rendering pipeline", "the DirectX 11 pipeline could not be created"));
		if (!CreateDummyResources())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"create dummy texture", "the fallback texture could not be created"));
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
				"resize swap chain", "the DirectX 11 buffers could not be resized", resizeResult, true));
		if (!renderTargets.Initialize(device.GetDevice(), device.GetSwapChain(),
			screenWidth, screenHeight, multiSampleCount, multiSampleQuality))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"resize render targets", "the DirectX 11 render targets could not be recreated"));
		if (postProcess.HasEffects() && !postProcess.InitializeTargets(device.GetDevice(),
			device.GetContext(), screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"resize post-process targets", "the DirectX 11 post-process targets could not be recreated"));
		Dx11DrawContext::ApplyViewport(device.GetContext(), screenWidth, screenHeight);
		return {};
	}

	GraphicsResult<FrameBeginState> Dx11Viewer::BeginFrameCore() {
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
					"draw post-process effects", "the DirectX 11 post-process chain failed"));
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
				"present swap chain", "the DirectX 11 frame could not be presented", presentResult, true));
		return FrameEndState::Presented;
	}

	GraphicsResult<void> Dx11Viewer::BeginPostProcessSceneInputPassCore() {
		if (!postProcess.BeginSceneInputPass(device.GetContext(),
			pipeline.GetDefaultDepthStencilState(), screenWidth, screenHeight)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"begin post-process scene input pass", "the DirectX 11 scene input pass could not begin"));
		}
		return {};
	}

	GraphicsResult<void> Dx11Viewer::EndPostProcessSceneInputPassCore() {
		if (device.GetContext() == nullptr)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"end post-process scene input pass", "the DirectX 11 context is unavailable"));
		postProcess.EndSceneInputPass(device.GetContext());
		return {};
	}

	GraphicsResult<void> Dx11Viewer::WaitIdle() {
		if (!device.WaitIdle())
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::SynchronizationFailed,
				"wait for GPU", "the DirectX 11 immediate context did not finish"));
		return {};
	}

	GraphicsResult<void> Dx11Viewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (!postProcess.Configure(device.GetDevice(), device.GetContext(), screenWidth, screenHeight, effects))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::EffectConfigurationFailed,
				"configure post-process effects", "the DirectX 11 effect chain could not be created"));
		return {};
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstanceCore() {
		return std::make_unique<Dx11Instance>(*this, device, textureCache, drawContext);
	}
}
