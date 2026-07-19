#include "Viewer/RenderTarget/Dx12PostProcessTarget.h"

namespace Chrivent {
	GraphicsResult<void> Dx12PostProcessTarget::Initialize(
		const Dx12Device& sourceDevice, const int width, const int height, const DXGI_FORMAT targetFormat) {
		Reset();
		if (!sourceDevice.GetDevice() || width <= 0 || height <= 0 || targetFormat == DXGI_FORMAT_UNKNOWN) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "후처리 target 생성",
				"DirectX 12 device, target 크기 또는 형식이 올바르지 않습니다"));
		}
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
		HRESULT result = sourceDevice.GetDevice()->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&resource));
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 target 생성",
				"DirectX 12 후처리 target을 만들지 못했습니다", result, true));
		}
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = 1;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		result = sourceDevice.GetDevice()->CreateDescriptorHeap(
			&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
		if (FAILED(result)) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 RTV heap 생성",
				"DirectX 12 후처리 RTV heap을 만들지 못했습니다", result, true));
		}
		sourceDevice.GetDevice()->CreateRenderTargetView(
			resource.Get(), nullptr, rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		return {};
	}

	void Dx12PostProcessTarget::Reset() {
		rtvDescriptorHeap.Reset();
		resource.Reset();
		format = DXGI_FORMAT_UNKNOWN;
	}
}
