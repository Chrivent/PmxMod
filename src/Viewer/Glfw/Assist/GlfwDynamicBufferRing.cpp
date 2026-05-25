#include "GlfwDynamicBufferRing.h"

namespace Chrivent {
	GlfwDynamicBufferRing::~GlfwDynamicBufferRing() {
		GlfwDynamicBufferRing::Clear();
	}

	bool GlfwDynamicBufferRing::Setup(
		const GLenum bufferTarget,
		const size_t bufferSize,
		const GLenum bufferUsage,
		std::string& outError) {
		Clear();
		if (!GlslDynamicBufferRing::Setup(bufferSize, outError))
			return false;
		target = bufferTarget;
		usage = bufferUsage;
		glGenBuffers(1, &buffer);
		if (buffer == 0) {
			outError = "Failed to create OpenGL dynamic buffer ring.";
			GlslDynamicBufferRing::Clear();
			return false;
		}
		glBindBuffer(target, buffer);
		glBufferData(target, static_cast<GLsizeiptr>(capacity), nullptr, usage);
		glBindBuffer(target, 0);
		return true;
	}

	void GlfwDynamicBufferRing::Clear() {
		if (buffer != 0) {
			glDeleteBuffers(1, &buffer);
			buffer = 0;
		}
		GlslDynamicBufferRing::Clear();
	}

	std::optional<GlslUploadSlice> GlfwDynamicBufferRing::Allocate(
		const size_t size,
		const size_t alignment,
		std::string& outError) {
		const size_t alignedOffset = AlignUp(writeOffset, alignment);
		if (alignedOffset + size > capacity) {
			outError = "OpenGL dynamic buffer ring is out of space for this frame.";
			return std::nullopt;
		}
		writeOffset = alignedOffset + size;
		outError.clear();
		return GlslUploadSlice{
			.offset = alignedOffset,
			.size = size,
			.cpuAddress = nullptr
		};
	}
}
