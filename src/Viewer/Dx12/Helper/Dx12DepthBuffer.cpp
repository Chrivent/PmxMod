#include "Viewer/Dx12/Helper/Dx12DepthBuffer.h"

namespace Chrivent {
	bool Dx12DepthBuffer::Initialize(const Dx12Device& sourceDevice, const int width, const int height) {
		Destroy();
		if (!sourceDevice.device || width <= 0 || height <= 0)
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		if (FAILED(sourceDevice.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dsvHeap))))
			return false;
		D3D12_CLEAR_VALUE clearValue;
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		resourceDesc.SampleDesc.Count = sourceDevice.msaaSampleCount;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		if (FAILED(sourceDevice.device->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue, IID_PPV_ARGS(&depthStencil))))
			return false;
		sourceDevice.device->CreateDepthStencilView(depthStencil.Get(), nullptr, ResolveDsvHandle());
		return true;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Dx12DepthBuffer::ResolveDsvHandle() const {
		if (!dsvHeap)
			return {};
		return dsvHeap->GetCPUDescriptorHandleForHeapStart();
	}

	void Dx12DepthBuffer::Destroy() {
		depthStencil.Reset();
		dsvHeap.Reset();
	}
}
