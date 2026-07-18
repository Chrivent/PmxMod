#include "Viewer/RenderTarget/Dx12PostProcessTarget.h"

namespace Chrivent {
	bool Dx12PostProcessTarget::Initialize(
		const Dx12Device& sourceDevice, const int width, const int height, const DXGI_FORMAT targetFormat) {
		Reset();
		if (!sourceDevice.GetDevice() || width <= 0 || height <= 0 || targetFormat == DXGI_FORMAT_UNKNOWN)
			return false;
		format = targetFormat;
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
		resourceDesc.Format = format;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = format;
		if (FAILED(sourceDevice.GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&resource))))
			return false;
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = 1;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		if (FAILED(sourceDevice.GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap))))
			return false;
		sourceDevice.GetDevice()->CreateRenderTargetView(
			resource.Get(), nullptr, rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		return true;
	}

	void Dx12PostProcessTarget::Reset() {
		rtvDescriptorHeap.Reset();
		resource.Reset();
		format = DXGI_FORMAT_UNKNOWN;
	}
}
