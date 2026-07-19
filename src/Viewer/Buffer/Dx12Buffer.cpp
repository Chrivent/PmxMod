#include "Viewer/Buffer/Dx12Buffer.h"

namespace Chrivent {
	GraphicsError::Result<void> Dx12Buffer::InitializeResource(
		const Dx12Device& sourceDevice, const size_t size,
		const D3D12_HEAP_TYPE heapType, const D3D12_RESOURCE_STATES initialState,
		const bool map) {
		Reset();
		if (!sourceDevice.GetDevice() || size == 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "buffer 생성",
				"DirectX 12 device 또는 buffer 크기가 올바르지 않습니다"));
		}
		D3D12_HEAP_PROPERTIES heapProperties;
		heapProperties.Type = heapType;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 1;
		heapProperties.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC resourceDesc{};
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Width = size;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		HRESULT result = sourceDevice.GetDevice()->CreateCommittedResource(
			&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
			initialState, nullptr, IID_PPV_ARGS(&resource));
		if (FAILED(result)) {
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ResourceCreationFailed, "buffer 생성",
				"DirectX 12 buffer를 만들지 못했습니다", result, true));
		}
		byteSize = size;
		if (!map)
			return {};
		constexpr D3D12_RANGE readRange{ 0, 0 };
		result = resource->Map(0, &readRange, &mappedData);
		if (SUCCEEDED(result))
			return {};
		Reset();
		return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
			GraphicsErrorCode::ResourceCreationFailed, "buffer 매핑",
			"DirectX 12 upload buffer를 영구 매핑하지 못했습니다", result, true));
	}

	GraphicsError::Result<void> Dx12Buffer::InitializeUpload(
		const Dx12Device& sourceDevice, const size_t size) {
		return InitializeResource(sourceDevice, size, D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ, true);
	}

	GraphicsError::Result<void> Dx12Buffer::InitializeDefault(
		const Dx12Device& sourceDevice, const size_t size,
		const D3D12_RESOURCE_STATES initialState) {
		return InitializeResource(
			sourceDevice, size, D3D12_HEAP_TYPE_DEFAULT, initialState, false);
	}

	bool Dx12Buffer::Write(const std::span<const std::byte> data, const size_t offset) const {
		if (!resource || mappedData == nullptr || offset > byteSize || data.size() > byteSize - offset)
			return false;
		if (data.empty())
			return true;
		std::memcpy(static_cast<std::byte*>(mappedData) + offset, data.data(), data.size());
		return true;
	}

	void Dx12Buffer::Reset() {
		if (resource && mappedData != nullptr)
			resource->Unmap(0, nullptr);
		resource.Reset();
		mappedData = nullptr;
		byteSize = 0;
	}

	void Dx12Buffer::Swap(Dx12Buffer& other) noexcept {
		resource.Swap(other.resource);
		std::swap(mappedData, other.mappedData);
		std::swap(byteSize, other.byteSize);
	}
}
