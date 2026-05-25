#include "VulkanDynamicBufferRing.h"

namespace Chrivent {
	bool VulkanDynamicBufferRing::Setup(const size_t bufferSize, std::string& outError) {
		mappedOffset = 0;
		return GlslDynamicBufferRing::Setup(bufferSize, outError);
	}

	void VulkanDynamicBufferRing::Clear() {
		mappedOffset = 0;
		GlslDynamicBufferRing::Clear();
	}

	std::optional<GlslUploadSlice> VulkanDynamicBufferRing::Allocate(
		const size_t size,
		const size_t alignment,
		std::string& outError) {
		const size_t alignedOffset = AlignUp(writeOffset, alignment);
		if (alignedOffset + size > capacity) {
			outError = "Vulkan dynamic buffer ring is out of space for this frame.";
			return std::nullopt;
		}
		writeOffset = alignedOffset + size;
		mappedOffset = alignedOffset;
		outError.clear();
		return GlslUploadSlice{
			.offset = alignedOffset,
			.size = size,
			.cpuAddress = nullptr
		};
	}
}
