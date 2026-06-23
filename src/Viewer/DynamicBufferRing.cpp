#include "Viewer/DynamicBufferRing.h"

namespace Chrivent {
	size_t DynamicBufferRing::AlignUp(const size_t value, const size_t alignment) {
		if (alignment <= 1)
			return value;
		const size_t remainder = value % alignment;
		if (remainder == 0)
			return value;
		return value + alignment - remainder;
	}

	bool DynamicBufferRing::Setup(const size_t bufferSize, std::string& outError) {
		Clear();
		if (bufferSize == 0) {
			outError = "Dynamic buffer ring size must be greater than zero.";
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
}
