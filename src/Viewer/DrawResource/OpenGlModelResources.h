#pragma once

#include "Viewer/Buffer/OpenGlDynamicBufferRing.h"

#include <glad/glad.h>
#include <vector>

namespace Chrivent {
	struct Material;

	// 공통 PMX 재질에 OpenGL 텍스처 객체와 알파 정보를 결합한다.
	struct OpenGlModelMaterial {
		const Material& material;
		GLuint texture = 0;
		bool textureHasAlpha = false;
		GLuint sphereTexture = 0;
		GLuint toonTexture = 0;

		explicit OpenGlModelMaterial(const Material& sourceMaterial) : material(sourceMaterial) {}
	};

	// OpenGL이 한 모델을 그릴 때 사용하는 GPU 리소스를 보관한다.
	struct OpenGlModelResources {
		GLenum indexType = GL_UNSIGNED_BYTE;
		GLuint vao = 0;
		GLuint edgeVao = 0;
		GLuint gsVao = 0;
		GLuint depthVao = 0;
		GLuint velocityVao = 0;
		size_t uniformBufferOffsetAlignment = 1;
		OpenGlDynamicBufferRing vertexConstantsRing;
		OpenGlDynamicBufferRing pixelConstantsRing;
		std::vector<OpenGlModelMaterial> materials;
	};
}
