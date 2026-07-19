#include "Viewer/Viewer/Dx11Viewer.h"

#include "Viewer/Instance/Dx11Instance.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	GraphicsResult<void> Dx11Viewer::CreateDummyResources() {
		const auto textureResult = textureCache.CreateWhiteTexture(device.GetDevice());
		if (!textureResult)
			return std::unexpected(textureResult.error());
		dummyTexture.texture = textureResult->texture;
		dummyTexture.textureView = textureResult->textureView;
		return {};
	}

	GraphicsResult<void> Dx11Viewer::SetupCore(const SceneShaderRuntimeContract& shaderContract) {
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

	GraphicsResult<void> Dx11Viewer::ResizeCore() {
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
				device.GetDevice(), device.GetContext(), screenWidth, screenHeight);
			if (!postProcessResult)
				return std::unexpected(postProcessResult.error());
		}
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
			const auto drawResult = postProcess.Draw(device.GetContext(), renderTargets.GetBackBufferView(),
				pipeline.GetPostProcessRasterizerState(), pipeline.GetToonTextureSampler(),
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
		const auto presentResult = swapChain.Present();
		if (!presentResult)
			return std::unexpected(presentResult.error());
		return FrameEndState::Presented;
	}

	GraphicsResult<void> Dx11Viewer::BeginPostProcessSceneInputPassCore() {
		return postProcess.BeginSceneInputPass(device.GetContext(),
			pipeline.GetDefaultDepthStencilState(), screenWidth, screenHeight);
	}

	GraphicsResult<void> Dx11Viewer::EndPostProcessSceneInputPassCore() {
		return postProcess.EndSceneInputPass(device.GetContext());
	}

	GraphicsResult<void> Dx11Viewer::WaitIdle() {
		return device.WaitIdle();
	}

	GraphicsResult<void> Dx11Viewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return postProcess.Configure(device.GetDevice(), device.GetContext(),
			screenWidth, screenHeight, effects);
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstanceCore() {
		return std::make_unique<Dx11Instance>(device, textureCache, drawContext);
	}
}
