#pragma once

#include "Viewer/DrawResource/OpenGlModelResources.h"
#include "Viewer/Instance/Instance.h"

#include <glad/glad.h>

namespace Chrivent {
	class OpenGlDrawContext;
	class OpenGlTextureCache;

	// 한 모델의 OpenGL 버퍼, VAO와 재질 상태를 관리한다.
	class OpenGlInstance : public Instance {
		OpenGlTextureCache& textureCache;
		OpenGlDrawContext& drawContext;
		OpenGlModelResources modelResources;
		GLuint vertexVbo = 0;
		GLuint ibo = 0;

		// OpenGL 버퍼를 생성하고 초기 데이터를 업로드한다.
		GraphicsError::Result<GLuint> CreateBuffer(size_t size, const void* data, GLenum usage) const;
		// 지정한 버퍼와 attribute 정보를 묶은 VAO를 생성한다.
		GraphicsError::Result<GLuint> CreateVao(GLuint vertexBuffer, const GLint* locations, const GLint* sizes,
			const size_t* offsets, int attributeCount, GLuint indexBuffer) const;
		// 모델 geometry 데이터를 OpenGL vertex/index buffer로 생성한다.
		GraphicsError::Result<void> CreateGeometryBuffers();
		// shader attribute 위치에 맞춰 모델/엣지/지면 그림자 VAO를 생성한다.
		GraphicsError::Result<void> CreateVertexArrays();
		// 패스별 uniform buffer ring을 material 개수에 맞춰 생성한다.
		GraphicsError::Result<void> SetupConstantRings();
		// 모델 material 정보를 OpenGL material 캐시와 texture handle로 변환한다.
		GraphicsError::Result<void> LoadMaterials();

	protected:
		// OpenGL 버퍼와 VAO 리소스를 초기 상태로 되돌린다.
		void ResetRendererResources() override;
		// OpenGL 모델 리소스를 생성하고 인스턴스를 초기화한다.
		GraphicsError::Result<void> SetupRenderer() override;
		// 모델의 갱신된 버텍스 데이터를 OpenGL 버퍼에 반영한다.
		GraphicsError::Result<void> UploadCore() override;

	public:
		OpenGlInstance(OpenGlTextureCache& sourceTextureCache, OpenGlDrawContext& sourceDrawContext);
		~OpenGlInstance() override;
	};
}
