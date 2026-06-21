#include "VulkanDynamicBufferRing.h"

namespace Chrivent {
	bool VulkanDynamicBufferRing::Setup(const VulkanDeviceInfo& deviceInfo, const size_t bufferSize, std::string& outError) {
		Clear();
		if (!GlslDynamicBufferRing::Setup(bufferSize, outError))
			return false;
		if (!buffer.Initialize(
			deviceInfo,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			outError = "Failed to create Vulkan dynamic buffer ring.";
			GlslDynamicBufferRing::Clear();
			return false;
		}
		return true;
	}

	void VulkanDynamicBufferRing::Clear() {
		buffer.Destroy();
		GlslDynamicBufferRing::Clear();
	}

	void VulkanDynamicBufferRing::BeginFrame(const size_t frameIndex) {
		GlslDynamicBufferRing::BeginFrame(frameIndex % kBufferedFrames);
	}

	std::optional<GlslUploadSlice> VulkanDynamicBufferRing::Allocate(const size_t size, const size_t alignment, std::string& outError) {
		const size_t frameCapacity = capacity / kBufferedFrames;
		if (frameCapacity == 0) {
			outError = "Vulkan dynamic buffer ring frame capacity is zero.";
			return std::nullopt;
		}
		const size_t alignedOffset = AlignUp(writeOffset, alignment);
		if (alignedOffset + size > frameCapacity) {
			outError = "Vulkan dynamic buffer ring is out of space for this frame.";
			return std::nullopt;
		}
		writeOffset = alignedOffset + size;
		outError.clear();
		const size_t frameBaseOffset = currentFrameIndex * frameCapacity;
		return GlslUploadSlice{
			.offset = frameBaseOffset + alignedOffset,
			.size = size,
			.cpuAddress = nullptr
		};
	}

	bool VulkanDynamicBufferRing::Write(const GlslUploadSlice& slice, const void* data, std::string& outError) const {
		if (data == nullptr) {
			outError = "Failed to write Vulkan dynamic buffer ring: source data is null.";
			return false;
		}
		if (!buffer.Write(data, slice.size, slice.offset)) {
			outError = "Failed to write Vulkan dynamic buffer ring slice.";
			return false;
		}
		outError.clear();
		return true;
	}
}
