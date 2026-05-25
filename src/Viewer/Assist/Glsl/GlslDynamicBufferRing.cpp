#include "GlslDynamicBufferRing.h"

namespace Chrivent {
	size_t GlslDynamicBufferRing::AlignUp(const size_t value, const size_t alignment) {
		if (alignment <= 1)
			return value;
		const size_t remainder = value % alignment;
		if (remainder == 0)
			return value;
		return value + (alignment - remainder);
	}

	bool GlslDynamicBufferRing::Setup(const size_t bufferSize, std::string& outError) {
		if (bufferSize == 0) {
			outError = "Dynamic buffer ring size must be greater than zero.";
			return false;
		}
		capacity = bufferSize;
		writeOffset = 0;
		currentFrameIndex = 0;
		outError.clear();
		return true;
	}

	void GlslDynamicBufferRing::Clear() {
		capacity = 0;
		writeOffset = 0;
		currentFrameIndex = 0;
	}

	void GlslDynamicBufferRing::BeginFrame(const size_t frameIndex) {
		currentFrameIndex = frameIndex;
		writeOffset = 0;
	}
}
