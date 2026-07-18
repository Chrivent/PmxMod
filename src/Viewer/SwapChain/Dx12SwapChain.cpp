#include "Viewer/SwapChain/Dx12SwapChain.h"

namespace Chrivent {
	bool Dx12SwapChain::CreateRenderTargetViews(const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetDevice() || !swapChain)
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = FrameBuffering::dx12BufferCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(sourceDevice.GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))))
			return false;
		rtvDescriptorSize = sourceDevice.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		for (UINT index = 0; index < FrameBuffering::dx12BufferCount; index++) {
			if (FAILED(swapChain->GetBuffer(index, IID_PPV_ARGS(&backBuffers[index]))))
				return false;
			sourceDevice.GetDevice()->CreateRenderTargetView(backBuffers[index].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize;
		}
		return true;
	}

	bool Dx12SwapChain::Initialize(const Dx12Device& sourceDevice, const HWND hwnd, const int width, const int height) {
		Reset();
		if (!sourceDevice.GetFactory() || !sourceDevice.GetDevice() || !sourceDevice.GetCommandQueue() || !hwnd)
			return false;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.BufferCount = FrameBuffering::dx12BufferCount;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;
		Microsoft::WRL::ComPtr<IDXGISwapChain1> baseSwapChain;
		if (FAILED(sourceDevice.GetFactory()->CreateSwapChainForHwnd(
			sourceDevice.GetCommandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &baseSwapChain)))
			return false;
		if (FAILED(sourceDevice.GetFactory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER)))
			return false;
		if (FAILED(baseSwapChain.As(&swapChain)))
			return false;
		frameIndex = swapChain->GetCurrentBackBufferIndex();
		return CreateRenderTargetViews(sourceDevice);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12SwapChain::GetCurrentRtvHandle() const {
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
		if (FAILED(swapChain->ResizeBuffers(
			FrameBuffering::dx12BufferCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0)))
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
