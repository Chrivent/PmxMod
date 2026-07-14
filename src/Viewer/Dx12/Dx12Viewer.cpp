#include "Viewer/Dx12/Dx12Viewer.h"

#include "Viewer/Dx12/Dx12Instance.h"
#include "Viewer/Dx12/Helper/Dx12Barrier.h"

#include <iostream>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	void Dx12Viewer::PrepareBackBufferForRendering(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) const {
		Dx12Barrier::Transition(commandList, commandContext.GetEnhancedCommandList().Get(), backBuffer,
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void Dx12Viewer::ClearRenderTargets(ID3D12GraphicsCommandList* commandList) const {
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = msaaColorBuffer.ResolveRtvHandle();
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthBuffer.ResolveDsvHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}

	void Dx12Viewer::ApplyViewportAndScissor(ID3D12GraphicsCommandList* commandList) const {
		D3D12_VIEWPORT viewport{};
		viewport.Width = screenWidth;
		viewport.Height = screenHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &viewport);
		D3D12_RECT scissorRect{};
		scissorRect.right = screenWidth;
		scissorRect.bottom = screenHeight;
		commandList->RSSetScissorRects(1, &scissorRect);
	}

	void Dx12Viewer::ResolveToBackBuffer(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer, ID3D12Resource* msaaColor) const {
		const D3D12_RESOURCE_STATES sourceState = device->msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_SOURCE
			: D3D12_RESOURCE_STATE_COPY_SOURCE;
		const D3D12_RESOURCE_STATES destinationState = device->msaaSampleCount > 1
			? D3D12_RESOURCE_STATE_RESOLVE_DEST
			: D3D12_RESOURCE_STATE_COPY_DEST;
		ID3D12GraphicsCommandList7* enhancedCommandList = commandContext.GetEnhancedCommandList().Get();
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			D3D12_RESOURCE_STATE_RENDER_TARGET, sourceState);
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, destinationState);
		if (device->msaaSampleCount > 1)
			commandList->ResolveSubresource(backBuffer, 0, msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(backBuffer, msaaColor);
		Dx12Barrier::Transition(commandList, enhancedCommandList, msaaColor,
			sourceState, D3D12_RESOURCE_STATE_RENDER_TARGET);
		Dx12Barrier::Transition(commandList, enhancedCommandList, backBuffer,
			destinationState, D3D12_RESOURCE_STATE_PRESENT);
	}

	Dx12Viewer::Dx12Viewer() {
		device = std::make_shared<Dx12Device>();
		dummyTexture = std::make_shared<Dx12Texture>();
	}

	Dx12Viewer::~Dx12Viewer() {
		commandContext.WaitForGpu(*device);
		postProcess.Reset();
		pipeline.Reset();
		commandContext.Reset();
		depthBuffer.Reset();
		msaaColorBuffer.Reset();
		swapChain.Reset();
		device->Shutdown();
	}

	void Dx12Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx12Viewer::Setup() {
		InitDirs("shaders/pmxmod-default/effects");
		if (!device->Initialize()) {
			std::cerr << "Failed to initialize DX12 device.\n";
			return false;
		}
		capabilities = device->capabilities;
		if (!commandContext.Initialize(*device)) {
			std::cerr << "Failed to initialize DX12 command context.\n";
			return false;
		}
		commandList = commandContext.GetCommandList();
		HWND__* hwnd = glfwGetWin32Window(window);
		if (!swapChain.Initialize(*device, hwnd, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 swap chain.\n";
			return false;
		}
		if (!msaaColorBuffer.Initialize(*device, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 MSAA color buffer.\n";
			return false;
		}
		if (!depthBuffer.Initialize(*device, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 depth buffer.\n";
			return false;
		}
		if (!postProcess.InitializeTargets(*device, screenWidth, screenHeight)) {
			std::cerr << "Failed to initialize DX12 post-process targets.\n";
			return false;
		}
		if (!pipeline.Initialize(*device, shaderDir)) {
			std::cerr << "Failed to initialize DX12 pipeline.\n";
			return false;
		}
		*dummyTexture = textureCache.CreateWhiteTexture(*device);
		if (!dummyTexture->resource) {
			std::cerr << "Failed to initialize DX12 dummy texture.\n";
			return false;
		}
		return true;
	}

	bool Dx12Viewer::Resize() {
		commandContext.WaitForGpu(*device);
		if (!swapChain.Resize(*device, screenWidth, screenHeight))
			return false;
		if (!msaaColorBuffer.Initialize(*device, screenWidth, screenHeight))
			return false;
		if (!depthBuffer.Initialize(*device, screenWidth, screenHeight))
			return false;
		if (!postProcess.InitializeTargets(*device, screenWidth, screenHeight))
			return false;
		return true;
	}

	void Dx12Viewer::BeginFrame() {
		frameReady = false;
		const UINT frameIndex = swapChain.GetFrameIndex();
		if (!commandContext.BeginFrame(*device, frameIndex))
			return;
		this->frameIndex = frameIndex;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.ResolveCurrentBackBuffer();
		const ID3D12Resource* msaaColor = msaaColorBuffer.ResolveResource();
		if (!commandList || !backBuffer || !msaaColor)
			return;
		PrepareBackBufferForRendering(commandList, backBuffer);
		ClearRenderTargets(commandList);
		ApplyViewportAndScissor(commandList);
		pipeline.BindModel(commandList, false);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		frameReady = true;
	}

	bool Dx12Viewer::EndFrame() {
		if (!frameReady)
			return false;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		ID3D12Resource* backBuffer = swapChain.ResolveCurrentBackBuffer();
		ID3D12Resource* msaaColor = msaaColorBuffer.ResolveResource();
		if (!commandList || !backBuffer || !msaaColor)
			return false;
		if (postProcess.HasEffects())
			postProcess.Draw(commandList, backBuffer, msaaColor,
				*device, commandContext, swapChain, screenWidth, screenHeight, postProcessFrameData);
		else
			ResolveToBackBuffer(commandList, backBuffer, msaaColor);
		if (!commandContext.Execute(*device))
			return false;
		if (!swapChain.Present())
			return false;
		frameReady = false;
		return true;
	}

	bool Dx12Viewer::BeginPostProcessDepthPass() {
		if (!frameReady)
			return false;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		return postProcess.BeginDepthPass(commandList, commandContext, screenWidth, screenHeight);
	}

	void Dx12Viewer::EndPostProcessDepthPass() {
		if (!frameReady)
			return;
		ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList().Get();
		postProcess.EndDepthPass(commandList, commandContext);
	}

	void Dx12Viewer::WaitIdle() {
		commandContext.WaitForGpu(*device);
	}

	bool Dx12Viewer::LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) {
		WaitIdle();
		const bool loaded = postProcess.Load(*device, effects);
		if (loaded)
			ResetPostProcessFrameHistory();
		return loaded;
	}

	void Dx12Viewer::ResetPostProcessHistory() {
		postProcess.ResetHistory();
		ResetPostProcessFrameHistory();
	}

	std::unique_ptr<Instance> Dx12Viewer::CreateInstance() const {
		return std::make_unique<Dx12Instance>();
	}

	Dx12Texture Dx12Viewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(*device, texturePath);
	}

	void Dx12Viewer::BindModelPipeline(const bool bothFace) const {
		if (!frameReady)
			return;
		pipeline.BindModel(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12Viewer::BindDepthOnlyPipeline(const bool bothFace) const {
		if (!frameReady)
			return;
		pipeline.BindDepthOnly(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12Viewer::BindSceneVelocityPipeline(const bool bothFace) const {
		if (!frameReady)
			return;
		pipeline.BindSceneVelocity(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12Viewer::BindEdgePipeline() const {
		if (!frameReady)
			return;
		pipeline.BindEdge(commandContext.GetCommandList().Get());
	}

	void Dx12Viewer::BindGroundShadowPipeline() const {
		if (!frameReady)
			return;
		pipeline.BindGroundShadow(commandContext.GetCommandList().Get());
	}
}
