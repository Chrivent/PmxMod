#include "Dx12SwapChain.h"

namespace Chrivent {
	bool Dx12SwapChain::Initialize(const Dx12DeviceInfo& deviceInfo, const HWND hwnd, const int width, const int height) {
		Destroy();
		if (!deviceInfo.factory || !deviceInfo.device || !deviceInfo.commandQueue || !hwnd)
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
		if (FAILED(deviceInfo.factory->CreateSwapChainForHwnd(
			deviceInfo.commandQueue.Get(),
			hwnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&baseSwapChain)))
			return false;
		if (FAILED(deviceInfo.factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER)))
			return false;
		if (FAILED(baseSwapChain.As(&swapChain)))
			return false;
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return CreateRenderTargetViews(deviceInfo);
	}

	bool Dx12SwapChain::Resize(const Dx12DeviceInfo& deviceInfo, const int width, const int height) {
		if (!swapChain || width <= 0 || height <= 0)
			return false;
		for (auto& backBuffer : backBuffers)
			backBuffer.Reset();
		if (FAILED(swapChain->ResizeBuffers(kFrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0)))
			return false;
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return CreateRenderTargetViews(deviceInfo);
	}

	bool Dx12SwapChain::Present() {
		if (!swapChain)
			return false;
		if (FAILED(swapChain->Present(0, 0)))
			return false;
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return true;
	}

	void Dx12SwapChain::Destroy() {
		for (auto& backBuffer : backBuffers)
			backBuffer.Reset();
		rtvHeap.Reset();
		swapChain.Reset();
		rtvDescriptorSize = 0;
		frameIndex = 0;
	}

	bool Dx12SwapChain::CreateRenderTargetViews(const Dx12DeviceInfo& deviceInfo) {
		if (!deviceInfo.device || !swapChain)
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = kFrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(deviceInfo.device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))))
			return false;
		rtvDescriptorSize = deviceInfo.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT index = 0; index < kFrameCount; index++) {
			if (FAILED(swapChain->GetBuffer(index, IID_PPV_ARGS(&backBuffers[index]))))
				return false;
			deviceInfo.device->CreateRenderTargetView(backBuffers[index].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize;
		}
		return true;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::GetCurrentRtvHandle() const {
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += frameIndex * rtvDescriptorSize;
		return rtvHandle;
	}
}
