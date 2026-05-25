#pragma once

#include "../../Assist/Glsl/GlslDynamicBufferRing.h"

#include <glad/glad.h>

namespace Chrivent {
	class GlfwDynamicBufferRing : public GlslDynamicBufferRing {
		GLenum target = GL_UNIFORM_BUFFER;
		GLenum usage = GL_DYNAMIC_DRAW;
		GLuint buffer = 0;

	public:
		~GlfwDynamicBufferRing() override;
		
		GLuint GetBuffer() const { return buffer; }
		GLenum GetTarget() const { return target; }

		// OpenGL 업로드 링 버퍼를 생성한다.
		bool Setup(GLenum bufferTarget, size_t bufferSize, GLenum bufferUsage, std::string& outError);
		void Clear() override;
		std::optional<GlslUploadSlice> Allocate(size_t size, size_t alignment, std::string& outError) override;
	};
}
