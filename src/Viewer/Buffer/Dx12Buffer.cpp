#include "Viewer/Buffer/Dx12Buffer.h"

namespace Chrivent {
	size_t Dx12Buffer::AlignConstantBufferSize(const size_t size) {
		constexpr size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		const size_t blockCount = (size + alignment - 1) / alignment;
		return blockCount * alignment;
	}

	D3D12_GPU_VIRTUAL_ADDRESS Dx12Buffer::ResolveGpuAddress() const {
		if (!resource)
			return 0;
		return resource->GetGPUVirtualAddress();
	}

	bool Dx12Buffer::InitializeUpload(const Dx12Device& sourceDevice, const size_t size) {
		Reset();
		if (!sourceDevice.device || size == 0)
			return false;
		D3D12_HEAP_PROPERTIES heapProperties;
		heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
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
		if (FAILED(sourceDevice.device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
			&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource))))
			return false;
		byteSize = size;
		constexpr D3D12_RANGE readRange{ 0, 0 };
		return SUCCEEDED(resource->Map(0, &readRange, &mappedData));
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
