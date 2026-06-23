#pragma once

#include "Viewer/DynamicBufferRing.h"

#include <glad/glad.h>

namespace Chrivent {
	class GlfwDynamicBufferRing : public DynamicBufferRing {
		GLenum usage = GL_DYNAMIC_DRAW;
		GLuint buffer = 0;

	public:
		~GlfwDynamicBufferRing() override;
		
		GLuint GetBuffer() const { return buffer; }

		// OpenGL 업로드 링 버퍼를 생성한다.
		bool Setup(size_t bufferSize, GLenum bufferUsage, std::string& outError);
		// OpenGL 업로드 링 버퍼와 공통 상태를 정리한다.
		void Clear() override;
		// 새 프레임에서 사용할 OpenGL 업로드 위치를 초기화한다.
		void BeginFrame(size_t frameIndex) override;
		// 지정한 크기와 정렬 조건에 맞는 OpenGL 업로드 구간을 예약한다.
		std::optional<UploadSlice> Allocate(size_t size, size_t alignment, std::string& outError) override;
	};
}
