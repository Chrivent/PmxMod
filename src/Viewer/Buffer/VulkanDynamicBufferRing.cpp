#include "Viewer/Buffer/VulkanDynamicBufferRing.h"

namespace Chrivent {
	bool VulkanDynamicBufferRing::Setup(const VulkanDevice& sourceDevice, const size_t bufferSize, std::string& outError) {
		Clear();
		if (!DynamicBufferRing::Setup(bufferSize, outError))
			return false;
		if (!buffer.Initialize(sourceDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			outError = "Failed to create Vulkan dynamic buffer ring.";
			DynamicBufferRing::Clear();
			return false;
		}
		return true;
	}

	void VulkanDynamicBufferRing::Clear() {
		buffer.Reset();
		DynamicBufferRing::Clear();
	}

	std::optional<UploadSlice> VulkanDynamicBufferRing::Allocate(const size_t size, const size_t alignment, std::string& outError) {
		const size_t frameCapacity = capacity / FrameBuffering::vulkanFramesInFlight;
		if (frameCapacity == 0) {
			outError = "Vulkan dynamic buffer ring frame capacity is zero.";
			return std::nullopt;
		}
		const size_t frameBaseOffset = currentFrameIndex * frameCapacity;
		return AllocateSlice(size, alignment, frameCapacity, frameBaseOffset,
			"Vulkan dynamic buffer ring is out of space for this frame.", outError);
	}

	bool VulkanDynamicBufferRing::Write(const UploadSlice& slice, const void* data,
		const size_t dataSize, std::string& outError) const {
		if (data == nullptr) {
			outError = "Failed to write Vulkan dynamic buffer ring: source data is null.";
			return false;
		}
		if (dataSize > slice.size) {
			outError = "Failed to write Vulkan dynamic buffer ring: source data exceeds the reserved slice.";
			return false;
		}
		if (!buffer.Write(data, dataSize, slice.offset)) {
			outError = "Failed to write Vulkan dynamic buffer ring slice.";
			return false;
		}
		outError.clear();
		return true;
	}
}
