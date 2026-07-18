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

	bool Dx11Viewer::SetupCore() {
		BindPostProcess(postProcess);
		HWND__* hwnd = glfwGetWin32Window(window);
		if (!device.Initialize(capabilities))
			return false;
		device.SelectMsaaSettings(multiSampleCount, multiSampleQuality, capabilities);
		capabilities.Print();
		if (!device.CreateSwapChain(hwnd, multiSampleCount, multiSampleQuality))
			return false;
		if (!renderTargets.Initialize(device.GetDevice(), device.GetSwapChain(),
			screenWidth, screenHeight, multiSampleCount, multiSampleQuality))
			return false;
		if (postProcess.HasEffects() && !postProcess.InitializeTargets(
			device.GetDevice(), device.GetContext(), screenWidth, screenHeight))
			return false;
		if (!pipeline.Initialize(device.GetDevice(), builtInShaderPasses, sceneInputShaderPasses))
			return false;
		if (!CreateDummyResources())
			return false;
		Dx11DrawContext::ApplyViewport(device.GetContext(), screenWidth, screenHeight);
		return true;
	}

	bool Dx11Viewer::ResizeCore() {
		renderTargets.Reset(device.GetContext());
		postProcess.ResetTargets();
		if (FAILED(device.GetSwapChain()->ResizeBuffers(0, screenWidth, screenHeight, DXGI_FORMAT_UNKNOWN, 0)))
			return false;
		if (!renderTargets.Initialize(device.GetDevice(), device.GetSwapChain(),
			screenWidth, screenHeight, multiSampleCount, multiSampleQuality))
			return false;
		if (postProcess.HasEffects() && !postProcess.InitializeTargets(
			device.GetDevice(), device.GetContext(), screenWidth, screenHeight))
			return false;
		Dx11DrawContext::ApplyViewport(device.GetContext(), screenWidth, screenHeight);
		return true;
	}

	FrameBeginResult Dx11Viewer::BeginFrameCore() {
		ID3D11RenderTargetView* sceneColorView = renderTargets.GetSceneColorView();
		device.GetContext()->ClearRenderTargetView(sceneColorView, clearColor);
		device.GetContext()->ClearDepthStencilView(renderTargets.GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		device.GetContext()->OMSetRenderTargets(1, &sceneColorView,
			renderTargets.GetDepthStencilView());
		pipeline.BindDefaultBlendState(device.GetContext());
		return FrameBeginResult::Ready;
	}

	FrameEndResult Dx11Viewer::EndFrameCore() {
		if (postProcess.HasEffects()) {
			postProcess.ResolveSceneColor(
				device.GetContext(), renderTargets.GetSceneColor(), multiSampleCount);
			if (!postProcess.Draw(device.GetContext(), renderTargets.GetBackBufferView(),
				pipeline.GetPostProcessRasterizerState(), pipeline.GetToonTextureSampler(),
				screenWidth, screenHeight, GetPostProcessFrameData())) {
				return FrameEndResult::Failed;
			}
		} else {
			if (multiSampleCount > 1)
				device.GetContext()->ResolveSubresource(renderTargets.GetBackBuffer(), 0,
					renderTargets.GetSceneColor(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
			else
				device.GetContext()->CopyResource(renderTargets.GetBackBuffer(),
					renderTargets.GetSceneColor());
		}
		if (FAILED(device.GetSwapChain()->Present(0, 0)))
			return FrameEndResult::Failed;
		return FrameEndResult::Presented;
	}

	bool Dx11Viewer::BeginPostProcessSceneInputPassCore() {
		return postProcess.BeginSceneInputPass(
			device.GetContext(), pipeline.GetDefaultDepthStencilState(), screenWidth, screenHeight);
	}

	bool Dx11Viewer::EndPostProcessSceneInputPassCore() {
		if (device.GetContext() == nullptr)
			return false;
		postProcess.EndSceneInputPass(device.GetContext());
		return true;
	}

	bool Dx11Viewer::WaitIdle() {
		return device.WaitIdle();
	}

	bool Dx11Viewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return postProcess.Configure(
			device.GetDevice(), device.GetContext(), screenWidth, screenHeight, effects);
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstanceCore() {
		return std::make_unique<Dx11Instance>(*this, device, textureCache, drawContext);
	}
}
