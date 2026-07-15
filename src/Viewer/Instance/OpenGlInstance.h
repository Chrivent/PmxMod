#pragma once

#include "Viewer/Instance/Instance.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Buffer/OpenGlDynamicBufferRing.h"

#include <vector>
#include <glad/glad.h>

namespace Chrivent {
    class OpenGlViewer;
    class OpenGlDrawer;
    struct OpenGlMaterial;

    // 한 모델의 OpenGL 버퍼, VAO와 재질 상태를 관리한다.
    class OpenGlInstance : public Instance {
    	GLuint vertexVbo = 0;
    	GLuint ibo = 0;
    	
		// OpenGL 버퍼를 생성하고 초기 데이터를 업로드한다.
		static GLuint CreateBuffer(size_t size, const void* data, GLenum usage);
		// 지정한 버퍼와 attribute 정보를 묶은 VAO를 생성한다.
		static GLuint CreateVao(GLuint vertexBuffer, const GLint* locations, const GLint* sizes, const size_t* offsets, int attributeCount, GLuint indexBuffer);
        // 모델 geometry 데이터를 OpenGL vertex/index buffer로 생성한다.
        bool CreateGeometryBuffers();
        // shader attribute 위치에 맞춰 모델/엣지/지면 그림자 VAO를 생성한다.
        void CreateVertexArrays();
        // 패스별 uniform buffer ring을 material 개수에 맞춰 생성한다.
        bool SetupConstantRings();
        // 모델 material 정보를 OpenGL material 캐시와 texture handle로 변환한다.
        void LoadMaterials();
    	
    protected:
		// OpenGL 모델 리소스를 생성하고 인스턴스를 초기화한다.
		bool SetupRenderer(Viewer& baseViewer) override;

    public:
        OpenGlViewer* viewer = nullptr;
        GLenum	indexType = GL_UNSIGNED_BYTE;
        GLuint	vao = 0;
        GLuint	edgeVao = 0;
        GLuint	gsVao = 0;
		GLuint depthVao = 0;
		GLuint velocityVao = 0;
        size_t uniformBufferOffsetAlignment = 1;
        OpenGlDynamicBufferRing vertexConstantsRing;
        OpenGlDynamicBufferRing pixelConstantsRing;
        std::vector<OpenGlMaterial> materials;

		OpenGlInstance();
        ~OpenGlInstance() override;

        // OpenGL 버퍼와 VAO 리소스를 해제한다.
        void Clear() override;
        // 모델의 갱신된 버텍스 데이터를 OpenGL 버퍼에 반영한다.
        void Upload() const override;
    };
}
