#include "Viewer/Buffer/DynamicBufferRing.h"

#include "Viewer/Buffer/BufferSize.h"

namespace Chrivent {
	bool DynamicBufferRing::Setup(const size_t bufferSize, std::string& outError) {
		Clear();
		if (bufferSize == 0) {
			outError = "Dynamic buffer ring 크기는 0보다 커야 합니다.";
			return false;
		}
		capacity = bufferSize;
		outError.clear();
		return true;
	}

	void DynamicBufferRing::Clear() {
		capacity = 0;
		writeOffset = 0;
		currentFrameIndex = 0;
	}

	void DynamicBufferRing::BeginFrame(const size_t frameIndex) {
		currentFrameIndex = frameIndex;
		writeOffset = 0;
	}

	std::optional<UploadSlice> DynamicBufferRing::AllocateSlice(const size_t size, const size_t alignment,
		const size_t availableCapacity, const size_t baseOffset, const char* capacityError,
		std::string& outError) {
		size_t alignedOffset = 0;
		size_t absoluteOffset = 0;
		if (!BufferSize::TryAlignUp(writeOffset, alignment, alignedOffset)
			|| size > availableCapacity || alignedOffset > availableCapacity - size
			|| !BufferSize::TryAdd(baseOffset, alignedOffset, absoluteOffset)) {
			outError = capacityError;
			return std::nullopt;
		}
		writeOffset = alignedOffset + size;
		outError.clear();
		return UploadSlice{
			.offset = absoluteOffset,
			.size = size,
			.cpuAddress = nullptr
		};
	}
}
