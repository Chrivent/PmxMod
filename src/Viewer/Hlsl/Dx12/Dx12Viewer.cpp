#include "Dx12Viewer.h"

#include "Dx12Instance.h"

#include <iostream>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	void Dx12Viewer::PrepareBackBufferForRendering(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = backBuffer;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
	}

	void Dx12Viewer::ClearRenderTargets(ID3D12GraphicsCommandList* commandList) const {
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = msaaColorBuffer.ResolveRtvHandle();
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthBuffer.ResolveDsvHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}

	void Dx12Viewer::SetViewportAndScissor(ID3D12GraphicsCommandList* commandList) const {
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
		D3D12_RESOURCE_BARRIER barriers[2]{};
		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Transition.pResource = msaaColor;
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[0].Transition.StateAfter = sourceState;
		barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Transition.pResource = backBuffer;
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[1].Transition.StateAfter = destinationState;
		barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(2, barriers);
		if (device->msaaSampleCount > 1)
			commandList->ResolveSubresource(backBuffer, 0, msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
		else
			commandList->CopyResource(backBuffer, msaaColor);
		barriers[0].Transition.StateBefore = sourceState;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barriers[1].Transition.StateBefore = destinationState;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		commandList->ResourceBarrier(2, barriers);
	}

	Dx12Viewer::Dx12Viewer() {
		device = std::make_shared<Dx12Device>();
		dummyTexture = std::make_shared<Dx12Texture>();
	}

	Dx12Viewer::~Dx12Viewer() {
		commandContext.WaitForGpu(*device);
		pipeline.Destroy();
		commandContext.Destroy();
		depthBuffer.Destroy();
		msaaColorBuffer.Destroy();
		swapChain.Destroy();
		device->Destroy();
	}

	void Dx12Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx12Viewer::Setup() {
		InitDirs("shader_hlsl");
		if (!device->Initialize()) {
			std::cerr << "Failed to initialize DX12 device.\n";
			return false;
		}
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
		return depthBuffer.Initialize(*device, screenWidth, screenHeight);
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
		SetViewportAndScissor(commandList);
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
		ResolveToBackBuffer(commandList, backBuffer, msaaColor);
		if (!commandContext.Execute(*device))
			return false;
		if (!swapChain.Present())
			return false;
		frameReady = false;
		return true;
	}

	void Dx12Viewer::WaitIdle() {
		commandContext.WaitForGpu(*device);
	}

	std::unique_ptr<Instance> Dx12Viewer::CreateInstance() const {
		return std::make_unique<Dx12Instance>();
	}

	Dx12Texture Dx12Viewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(*device, texturePath);
	}

	void Dx12Viewer::BindModelPipelineState(const bool bothFace) const {
		if (!frameReady)
			return;
		pipeline.BindModel(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12Viewer::BindEdgePipelineState() const {
		if (!frameReady)
			return;
		pipeline.BindEdge(commandContext.GetCommandList().Get());
	}

	void Dx12Viewer::BindGroundShadowPipelineState() const {
		if (!frameReady)
			return;
		pipeline.BindGroundShadow(commandContext.GetCommandList().Get());
	}
}
