#include "Viewer/Buffer/OpenGlDynamicBufferRing.h"

#include "Viewer/Error/OpenGlErrorState.h"

namespace Chrivent {
	OpenGlDynamicBufferRing::~OpenGlDynamicBufferRing() {
		Clear();
	}

	GraphicsError::Result<void> OpenGlDynamicBufferRing::Setup(
		const size_t bufferSize, const GLenum bufferUsage) {
		Clear();
		const auto setupResult = DynamicBufferRing::Setup(bufferSize);
		if (!setupResult) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::InvalidArgument, "동적 uniform buffer ring 생성",
				setupResult.error().message));
		}
		usage = bufferUsage;
		OpenGlErrorState::Clear();
		glCreateBuffers(1, &buffer);
		const GLenum createResult = OpenGlErrorState::Take();
		if (buffer == 0 || createResult != GL_NO_ERROR) {
			Clear();
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "동적 uniform buffer ring 생성",
				"OpenGL buffer를 만들지 못했습니다",
				createResult, createResult != GL_NO_ERROR));
		}
		OpenGlErrorState::Clear();
		glNamedBufferData(buffer, capacity, nullptr, usage);
		const GLenum result = OpenGlErrorState::Take();
		if (result == GL_NO_ERROR)
			return {};
		Clear();
		return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
			GraphicsErrorCode::ResourceCreationFailed, "동적 uniform buffer storage 생성",
			"OpenGL uniform buffer storage를 할당하지 못했습니다", result, true));
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

	DynamicBufferError::Result<UploadSlice> OpenGlDynamicBufferRing::Allocate(
		const size_t size, const size_t alignment) {
		return AllocateSlice(size, alignment, capacity, 0,
			"현재 프레임에서 OpenGL 동적 버퍼 링의 남은 공간이 부족합니다.");
	}
}
