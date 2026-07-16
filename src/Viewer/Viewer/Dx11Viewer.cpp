#include "Viewer/Viewer/Dx11Viewer.h"

#include "Viewer/Instance/Dx11Instance.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	bool Dx11Viewer::CreateDummyResources() {
		const Dx11Texture texture = textureCache.CreateWhiteTexture(device.ResolveDevice());
		dummyTexture.texture = texture.texture;
		dummyTexture.textureView = texture.textureView;
		return dummyTexture.texture && dummyTexture.textureView;
	}

	void Dx11Viewer::ConfigureWindowHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx11Viewer::SetupCore() {
		BindPostProcess(postProcess);
		HWND__* hwnd = glfwGetWin32Window(window);
		if (!device.Initialize(capabilities))
			return false;
		device.ResolveMsaaSettings(multiSampleCount, multiSampleQuality, capabilities);
		capabilities.Print();
		if (!device.CreateSwapChain(hwnd, multiSampleCount, multiSampleQuality))
			return false;
		if (!renderTargets.Initialize(device.ResolveDevice(), device.ResolveContext(),
			device.ResolveSwapChain(), postProcess, screenWidth, screenHeight,
			multiSampleCount, multiSampleQuality))
			return false;
		if (!pipeline.Initialize(device.ResolveDevice(), builtInShaderPasses, sceneInputShaderPasses))
			return false;
		if (!CreateDummyResources())
			return false;
		Dx11DrawContext::ApplyViewport(device.ResolveContext(), screenWidth, screenHeight);
		return true;
	}

	bool Dx11Viewer::ResizeCore() {
		renderTargets.Reset(device.ResolveContext(), postProcess);
		if (FAILED(device.ResolveSwapChain()->ResizeBuffers(0, screenWidth, screenHeight, DXGI_FORMAT_UNKNOWN, 0)))
			return false;
		if (!renderTargets.Initialize(device.ResolveDevice(), device.ResolveContext(),
			device.ResolveSwapChain(), postProcess, screenWidth, screenHeight,
			multiSampleCount, multiSampleQuality))
			return false;
		Dx11DrawContext::ApplyViewport(device.ResolveContext(), screenWidth, screenHeight);
		return true;
	}

	FrameBeginResult Dx11Viewer::BeginFrameCore() {
		ID3D11RenderTargetView* sceneColorView = renderTargets.ResolveSceneColorView();
		device.ResolveContext()->ClearRenderTargetView(sceneColorView, clearColor);
		device.ResolveContext()->ClearDepthStencilView(renderTargets.ResolveDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		device.ResolveContext()->OMSetRenderTargets(1, &sceneColorView,
			renderTargets.ResolveDepthStencilView());
		device.ResolveContext()->OMSetBlendState(pipeline.GetStates().blendState.Get(), nullptr, 0xffffffff);
		return FrameBeginResult::Ready;
	}

	FrameEndResult Dx11Viewer::EndFrameCore() {
		if (postProcess.HasEffects()) {
			postProcess.ResolveSceneColor(
				device.ResolveContext(), renderTargets.ResolveSceneColor(), multiSampleCount);
			if (!postProcess.Draw(device.ResolveContext(), renderTargets.ResolveBackBufferView(),
				pipeline.GetStates().bothFaceRs.Get(), pipeline.GetStates().toonTextureSampler.Get(),
				screenWidth, screenHeight, GetPostProcessFrameData())) {
				return FrameEndResult::Failed;
			}
		} else {
			device.ResolveContext()->CopyResource(renderTargets.ResolveBackBuffer(),
				renderTargets.ResolveSceneColor());
		}
		if (FAILED(device.ResolveSwapChain()->Present(0, 0)))
			return FrameEndResult::Failed;
		return FrameEndResult::Presented;
	}

	bool Dx11Viewer::BeginPostProcessSceneInputPassCore() {
		return postProcess.BeginSceneInputPass(
			device.ResolveContext(), pipeline.GetStates().defaultDss.Get(), screenWidth, screenHeight);
	}

	bool Dx11Viewer::EndPostProcessSceneInputPassCore() {
		if (device.ResolveContext() == nullptr)
			return false;
		postProcess.EndSceneInputPass(device.ResolveContext());
		return true;
	}

	bool Dx11Viewer::WaitIdle() {
		return device.WaitIdle();
	}

	bool Dx11Viewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return postProcess.Load(device.ResolveDevice(), effects);
	}

	std::unique_ptr<Instance> Dx11Viewer::CreateInstanceCore() {
		return std::make_unique<Dx11Instance>(*this);
	}
}
