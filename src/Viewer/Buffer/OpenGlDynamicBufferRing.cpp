#include "Viewer/Buffer/OpenGlDynamicBufferRing.h"

namespace Chrivent {
	OpenGlDynamicBufferRing::~OpenGlDynamicBufferRing() {
		OpenGlDynamicBufferRing::Clear();
	}

	bool OpenGlDynamicBufferRing::Setup(const size_t bufferSize, const GLenum bufferUsage, std::string& outError) {
		Clear();
		if (!DynamicBufferRing::Setup(bufferSize, outError))
			return false;
		usage = bufferUsage;
		glCreateBuffers(1, &buffer);
		if (buffer == 0) {
			outError = "OpenGL dynamic buffer ring을 만들지 못했습니다.";
			DynamicBufferRing::Clear();
			return false;
		}
		glNamedBufferData(buffer, capacity, nullptr, usage);
		return true;
	}

	void OpenGlDynamicBufferRing::Clear() {
		if (buffer != 0) {
			glDeleteBuffers(1, &buffer);
			buffer = 0;
		}
		DynamicBufferRing::Clear();
	}

	void OpenGlDynamicBufferRing::BeginFrame(const size_t frameIndex) {
		DynamicBufferRing::BeginFrame(frameIndex);
		if (buffer == 0)
			return;
		glNamedBufferData(buffer, capacity, nullptr, usage);
	}

	std::optional<UploadSlice> OpenGlDynamicBufferRing::Allocate(const size_t size, const size_t alignment, std::string& outError) {
		return AllocateSlice(size, alignment, capacity, 0,
			"OpenGL dynamic buffer ring is out of space for this frame.", outError);
	}
}
