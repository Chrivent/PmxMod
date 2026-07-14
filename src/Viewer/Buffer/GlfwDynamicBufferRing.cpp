#include "Viewer/Buffer/GlfwDynamicBufferRing.h"

namespace Chrivent {
	GlfwDynamicBufferRing::~GlfwDynamicBufferRing() {
		GlfwDynamicBufferRing::Clear();
	}

	bool GlfwDynamicBufferRing::Setup(const size_t bufferSize, const GLenum bufferUsage, std::string& outError) {
		Clear();
		if (!DynamicBufferRing::Setup(bufferSize, outError))
			return false;
		usage = bufferUsage;
		glCreateBuffers(1, &buffer);
		if (buffer == 0) {
			outError = "Failed to create OpenGL dynamic buffer ring.";
			DynamicBufferRing::Clear();
			return false;
		}
		glNamedBufferData(buffer, capacity, nullptr, usage);
		return true;
	}

	void GlfwDynamicBufferRing::Clear() {
		if (buffer != 0) {
			glDeleteBuffers(1, &buffer);
			buffer = 0;
		}
		DynamicBufferRing::Clear();
	}

	void GlfwDynamicBufferRing::BeginFrame(const size_t frameIndex) {
		DynamicBufferRing::BeginFrame(frameIndex);
		if (buffer == 0)
			return;
		glNamedBufferData(buffer, capacity, nullptr, usage);
	}

	std::optional<UploadSlice> GlfwDynamicBufferRing::Allocate(const size_t size, const size_t alignment, std::string& outError) {
		const size_t alignedOffset = AlignUp(writeOffset, alignment);
		if (alignedOffset + size > capacity) {
			outError = "OpenGL dynamic buffer ring is out of space for this frame.";
			return std::nullopt;
		}
		writeOffset = alignedOffset + size;
		outError.clear();
		return UploadSlice{
			.offset = alignedOffset,
			.size = size,
			.cpuAddress = nullptr
		};
	}
}
