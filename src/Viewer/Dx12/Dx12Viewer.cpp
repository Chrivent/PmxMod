#include "Dx12Viewer.h"

#include "Dx12Instance.h"

#include <iostream>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	Dx12Viewer::Dx12Viewer() {
		info = std::make_unique<Dx12ViewerInfo>();
	}

	Dx12Viewer::~Dx12Viewer() {
		commandContext.WaitForGpu(device.GetInfo());
		pipeline.Destroy();
		commandContext.Destroy();
		swapChain.Destroy();
		device.Destroy();
	}

	void Dx12Viewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	bool Dx12Viewer::Setup() {
		InitDirs("shader_hlsl");
		if (!device.Initialize()) {
			std::cerr << "Failed to initialize DX12 device.\n";
			return false;
		}
		if (!commandContext.Initialize(device.GetInfo())) {
			std::cerr << "Failed to initialize DX12 command context.\n";
			return false;
		}
		HWND__* hwnd = glfwGetWin32Window(GetInfo().window);
		if (!swapChain.Initialize(device.GetInfo(), hwnd, GetInfo().screenWidth, GetInfo().screenHeight)) {
			std::cerr << "Failed to initialize DX12 swap chain.\n";
			return false;
		}
		if (!pipeline.Initialize(device.GetInfo(), GetInfo().shaderDir)) {
			std::cerr << "Failed to initialize DX12 pipeline.\n";
			return false;
		}
		dummyTexture = textureCache.CreateWhiteTexture(device.GetInfo());
		if (!dummyTexture.resource) {
			std::cerr << "Failed to initialize DX12 dummy texture.\n";
			return false;
		}
		return true;
	}

	bool Dx12Viewer::Resize() {
		commandContext.WaitForGpu(device.GetInfo());
		return swapChain.Resize(device.GetInfo(), GetInfo().screenWidth, GetInfo().screenHeight);
	}

	void Dx12Viewer::BeginFrame() {
		if (!commandContext.BeginFrame())
			return;
		ID3D12GraphicsCommandList* commandList = commandContext.GetInfo().commandList.Get();
		ID3D12Resource* backBuffer = swapChain.GetCurrentBackBuffer();
		if (!commandList || !backBuffer)
			return;
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = backBuffer;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = swapChain.CalculateCurrentRtvHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->SetGraphicsRootSignature(pipeline.GetModelRootSignature());
		commandList->SetPipelineState(pipeline.GetModelPipelineState());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	bool Dx12Viewer::EndFrame() {
		ID3D12GraphicsCommandList* commandList = commandContext.GetInfo().commandList.Get();
		ID3D12Resource* backBuffer = swapChain.GetCurrentBackBuffer();
		if (!commandList || !backBuffer)
			return false;
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = backBuffer;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		if (!commandContext.Execute(device.GetInfo()))
			return false;
		if (!swapChain.Present())
			return false;
		return commandContext.WaitForGpu(device.GetInfo());
	}

	std::unique_ptr<Instance> Dx12Viewer::CreateInstance() const {
		return std::make_unique<Dx12Instance>();
	}

	Dx12Texture Dx12Viewer::LoadTexture(const std::filesystem::path& texturePath) {
		return textureCache.Load(device.GetInfo(), texturePath);
	}
}
