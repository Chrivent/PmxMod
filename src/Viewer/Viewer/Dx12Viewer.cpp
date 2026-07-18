#include "Viewer/Viewer/Dx12Viewer.h"

#include "Viewer/Instance/Dx12Instance.h"
#include "Viewer/Synchronization/Dx12Barrier.h"

#include <iostream>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	void Dx12Viewer::PrepareBackBufferForRendering(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) const {
		Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(), backBuffer,
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Viewer::ClearRenderTargets(ID3D12GraphicsCommandList* commandList) const {
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = msaaColorBuffer.GetRtvHandle();
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthBuffer.GetDsvHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}

	bool Dx12Viewer::SetupCore() {
		BindPostProcess(postProcess);
		if (!device.Initialize()) {
			std::cerr << "Failed to initialize DX12 device.\n";
			return false;
		}
		capabilities = device.capabilities;
		if (!commandContext.Initialize(device)) {
			std::cerr << "Failed to initialize DX12 command context.\n";
			return false;
		}
		HWND__* hwnd = glfwGetWin32Window(window);
		if (!swapChain.Initialize(device, hwnd, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 swap chain.\n";
			return false;
		}
		if (!msaaColorBuffer.Initialize(device, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 MSAA color buffer.\n";
			return false;
		}
		if (!depthBuffer.Initialize(device, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 depth buffer.\n";
			return false;
		}
		if (postProcess.HasEffects() && !postProcess.InitializeTargets(device, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 post-process targets.\n";
			return false;
		}
		if (!pipeline.Initialize(device, builtInShaderPasses,
			sceneInputShaderPasses.depth, sceneInputShaderPasses.velocity)) {
			std::cerr << "Failed to initialize DX12 pipeline.\n";
			return false;
		}
		dummyTexture = textureCache.CreateWhiteTexture(device);
		if (!dummyTexture.resource) {
			std::cerr << "Failed to initialize DX12 dummy texture.\n";
			return false;
		}
		return true;
	}

	bool Dx12Viewer::ResizeCore() {
		if (!WaitIdle())
			return false;
		if (!swapChain.Resize(device, screenWidth, screenHeight))
			return false;
		if (!msaaColorBuffer.Initialize(device, screenWidth, screenHeight))
			return false;
		if (!depthBuffer.Initialize(device, screenWidth, screenHeight))
			return false;
		if (postProcess.HasEffects()) {
			if (!postProcess.InitializeTargets(device, screenWidth, screenHeight))
				return false;
		} else
			postProcess.ResetResources();
		return true;
	}

	FrameBeginResult Dx12Viewer::BeginFrameCore() {
		drawContext.EndFrame();
		const UINT frameIndex = swapChain.GetFrameIndex();
		if (!commandContext.BeginFrame(device, frameIndex))
			return FrameBeginResult::Failed;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.GetCurrentBackBuffer();
		const ID3D12Resource* msaaColor = msaaColorBuffer.GetResource();
		if (!commandList || !backBuffer || !msaaColor)
			return FrameBeginResult::Failed;
		PrepareBackBufferForRendering(commandList, backBuffer);
		ClearRenderTargets(commandList);
		Dx12CommandContext::ApplyViewportAndScissor(commandList, screenWidth, screenHeight);
		pipeline.BindModel(commandList, false);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		drawContext.BeginFrame(frameIndex);
		return FrameBeginResult::Ready;
	}

	FrameEndResult Dx12Viewer::EndFrameCore() {
		if (!drawContext.IsFrameReady())
			return FrameEndResult::Failed;
		drawContext.EndFrame();
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.GetCurrentBackBuffer();
		const ID3D12Resource* msaaColor = msaaColorBuffer.GetResource();
		if (!commandList || !backBuffer || !msaaColor)
			return FrameEndResult::Failed;
		bool drawSucceeded;
		if (postProcess.HasEffects())
			drawSucceeded = postProcess.Draw(commandList, backBuffer, msaaColorBuffer,
				device, commandContext, swapChain, screenWidth, screenHeight, GetPostProcessFrameData());
		else
			drawSucceeded = msaaColorBuffer.ResolveToBackBuffer(
				commandList, commandContext.GetEnhancedCommandList().Get(), backBuffer);
		if (!commandContext.Execute(device))
			return FrameEndResult::Failed;
		if (!swapChain.Present())
			return FrameEndResult::Failed;
		if (!drawSucceeded)
			return FrameEndResult::Failed;
		return FrameEndResult::Presented;
	}

	bool Dx12Viewer::BeginPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return false;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		return postProcess.BeginSceneInputPass(commandList, commandContext, screenWidth, screenHeight);
	}

	bool Dx12Viewer::EndPostProcessSceneInputPassCore() {
		if (!drawContext.IsFrameReady())
			return false;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		return postProcess.EndSceneInputPass(commandList, commandContext);
	}

	bool Dx12Viewer::WaitIdle() {
		return device.device && commandContext.WaitForGpu(device);
	}

	bool Dx12Viewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return device.device && postProcess.Configure(device, screenWidth, screenHeight, effects);
	}

	std::unique_ptr<Instance> Dx12Viewer::CreateInstanceCore() {
		return std::make_unique<Dx12Instance>(*this, device, textureCache, dummyTexture, drawContext);
	}

}
