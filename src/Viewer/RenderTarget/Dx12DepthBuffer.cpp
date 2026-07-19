#include "Viewer/RenderTarget/Dx12DepthBuffer.h"

namespace Chrivent {
	GraphicsError::Result<void> Dx12DepthBuffer::Initialize(
		const Dx12Device& sourceDevice, const int width, const int height) {
		Reset();
		if (!sourceDevice.GetDevice() || width <= 0 || height <= 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "depth buffer 생성",
				"DirectX 12 device 또는 depth buffer 크기가 올바르지 않습니다"));
		}
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		HRESULT result = sourceDevice.GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&dsvHeap));
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "depth descriptor heap 생성",
				"DirectX 12 depth descriptor heap을 만들지 못했습니다", result, true));
		}
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
		resourceDesc.SampleDesc.Count = sourceDevice.GetMsaaSampleCount();
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		result = sourceDevice.GetDevice()->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue, IID_PPV_ARGS(&depthStencil));
		if (FAILED(result)) {
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "depth buffer 생성",
				"DirectX 12 depth buffer를 만들지 못했습니다", result, true));
		}
		sourceDevice.GetDevice()->CreateDepthStencilView(depthStencil.Get(), nullptr, GetDsvHandle());
		return {};
	}

	void Dx12DepthBuffer::Reset() {
		depthStencil.Reset();
		dsvHeap.Reset();
	}
}
