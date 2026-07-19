#include "Viewer/RenderTarget/Dx12PostProcessDepthTarget.h"

namespace Chrivent {
	GraphicsError::Result<void> Dx12PostProcessDepthTarget::Initialize(const Dx12Device& sourceDevice,
		const int width, const int height) {
		Reset();
		if (!sourceDevice.GetDevice() || width <= 0 || height <= 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "후처리 depth target 생성",
				"DirectX 12 device 또는 depth target 크기가 올바르지 않습니다"));
		}
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC description{};
		description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		description.Width = width;
		description.Height = height;
		description.DepthOrArraySize = 1;
		description.MipLevels = 1;
		description.Format = DXGI_FORMAT_R24G8_TYPELESS;
		description.SampleDesc.Count = 1;
		description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clearValue.DepthStencil.Depth = 1.0f;
		HRESULT result = sourceDevice.GetDevice()->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &description,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&resource));
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 depth target 생성",
				"DirectX 12 후처리 depth target을 만들지 못했습니다", result, true));
		}
		D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
		heapDescription.NumDescriptors = 1;
		heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		result = sourceDevice.GetDevice()->CreateDescriptorHeap(
			&heapDescription, IID_PPV_ARGS(&dsvHeap));
		if (FAILED(result)) {
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 depth descriptor heap 생성",
				"DirectX 12 후처리 depth descriptor heap을 만들지 못했습니다", result, true));
		}
		D3D12_DEPTH_STENCIL_VIEW_DESC viewDescription{};
		viewDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		viewDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		sourceDevice.GetDevice()->CreateDepthStencilView(
			resource.Get(), &viewDescription, GetDsvHandle());
		return {};
	}

	void Dx12PostProcessDepthTarget::Reset() {
		dsvHeap.Reset();
		resource.Reset();
	}

	void Dx12PostProcessDepthTarget::Swap(Dx12PostProcessDepthTarget& other) noexcept {
		resource.Swap(other.resource);
		dsvHeap.Swap(other.dsvHeap);
	}
}
