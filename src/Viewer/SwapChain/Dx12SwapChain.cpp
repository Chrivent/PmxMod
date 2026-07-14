#include "Viewer/SwapChain/Dx12SwapChain.h"

namespace Chrivent {
	bool Dx12SwapChain::CreateRenderTargetViews(const Dx12Device& sourceDevice) {
		if (!sourceDevice.device || !swapChain)
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = kFrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))))
			return false;
		rtvDescriptorSize = sourceDevice.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT index = 0; index < kFrameCount; index++) {
			if (FAILED(swapChain->GetBuffer(index, IID_PPV_ARGS(&backBuffers[index]))))
				return false;
			sourceDevice.device->CreateRenderTargetView(backBuffers[index].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize;
		}
		return true;
	}

	bool Dx12SwapChain::Initialize(const Dx12Device& sourceDevice, const HWND hwnd, const int width, const int height) {
		Reset();
		if (!sourceDevice.factory || !sourceDevice.device || !sourceDevice.commandQueue || !hwnd)
			return false;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.BufferCount = kFrameCount;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;
		Microsoft::WRL::ComPtr<IDXGISwapChain1> baseSwapChain;
		if (FAILED(sourceDevice.factory->CreateSwapChainForHwnd(
			sourceDevice.commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &baseSwapChain)))
			return false;
		if (FAILED(sourceDevice.factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER)))
			return false;
		if (FAILED(baseSwapChain.As(&swapChain)))
			return false;
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return CreateRenderTargetViews(sourceDevice);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::ResolveCurrentRtvHandle() const {
		if (!rtvHeap)
			return {};
		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(frameIndex) * rtvDescriptorSize;
		return handle;
	}

	bool Dx12SwapChain::Resize(const Dx12Device& sourceDevice, const int width, const int height) {
		if (!swapChain || width <= 0 || height <= 0)
			return false;
		for (auto& backBuffer : backBuffers)
			backBuffer.Reset();
		if (FAILED(swapChain->ResizeBuffers(kFrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0)))
			return false;
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return CreateRenderTargetViews(sourceDevice);
	}

	bool Dx12SwapChain::Present() {
		if (!swapChain)
			return false;
		if (FAILED(swapChain->Present(0, 0)))
			return false;
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return true;
	}

	void Dx12SwapChain::Reset() {
		for (auto& backBuffer : backBuffers)
			backBuffer.Reset();
		rtvHeap.Reset();
		swapChain.Reset();
		rtvDescriptorSize = 0;
		frameIndex = 0;
	}
}
