#include "Dx12Buffer.h"

#include <cstring>

namespace Chrivent {
	D3D12_GPU_VIRTUAL_ADDRESS Dx12Buffer::ResolveGpuAddress() const {
		if (!resource)
			return 0;
		return resource->GetGPUVirtualAddress();
	}

	size_t Dx12Buffer::AlignConstantBufferSize(const size_t size) {
		constexpr size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
		const size_t blockCount = (size + alignment - 1) / alignment;
		return blockCount * alignment;
	}

	bool Dx12Buffer::InitializeUpload(const Dx12DeviceInfo& deviceInfo, const size_t size) {
		Destroy();
		if (!deviceInfo.device || size == 0)
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
		if (FAILED(deviceInfo.device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource))))
			return false;
		byteSize = size;
		return true;
	}

	bool Dx12Buffer::Write(const void* data, const size_t size) const {
		if (!resource || data == nullptr || size > byteSize)
			return false;
		void* mappedData = nullptr;
		constexpr D3D12_RANGE readRange{ 0, 0 };
		if (FAILED(resource->Map(0, &readRange, &mappedData)))
			return false;
		std::memcpy(mappedData, data, size);
		resource->Unmap(0, nullptr);
		return true;
	}

	void Dx12Buffer::Destroy() {
		resource.Reset();
		byteSize = 0;
	}
}
