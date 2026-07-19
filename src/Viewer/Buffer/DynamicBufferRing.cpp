#include "Viewer/Buffer/DynamicBufferRing.h"

#include "Viewer/Buffer/BufferSize.h"

#include <utility>

namespace Chrivent {
	DynamicBufferError::Result<void> DynamicBufferRing::Setup(const size_t bufferSize) {
		Clear();
		if (bufferSize == 0)
			return std::unexpected(DynamicBufferError{
				.message = "Dynamic buffer ring 크기는 0보다 커야 합니다."
			});
		capacity = bufferSize;
		return {};
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

	DynamicBufferError::Result<UploadSlice> DynamicBufferRing::AllocateSlice(
		const size_t size, const size_t alignment, const size_t availableCapacity,
		const size_t baseOffset, std::string capacityError) {
		size_t alignedOffset = 0;
		size_t absoluteOffset = 0;
		if (!BufferSize::TryAlignUp(writeOffset, alignment, alignedOffset)
			|| size > availableCapacity || alignedOffset > availableCapacity - size
			|| !BufferSize::TryAdd(baseOffset, alignedOffset, absoluteOffset)) {
			return std::unexpected(DynamicBufferError{
				.message = std::move(capacityError)
			});
		}
		writeOffset = alignedOffset + size;
		return UploadSlice{
			.offset = absoluteOffset,
			.size = size,
			.cpuAddress = nullptr
		};
	}
}
