#pragma once

#include "../Instance.h"

#include <glad/glad.h>

namespace Chrivent {
    class GlfwViewer;
    class GlfwDrawer;
    struct GlfwViewerMaterial;

    struct GlfwInstanceInfo : InstanceInfo {
        GlfwViewer* viewer = nullptr;
        GLenum	indexType = GL_UNSIGNED_BYTE;
        GLuint	vao = 0;
        GLuint	edgeVao = 0;
        GLuint	gsVao = 0;
        std::vector<GlfwViewerMaterial> materials;
    };

    class GlfwInstance : public Instance {
    protected:
        // OpenGL 버퍼를 생성하고 초기 데이터를 업로드한다.
        static GLuint CreateBuffer(GLenum target, size_t size, const void* data, GLenum usage);
        // 지정한 버퍼와 attribute 정보를 묶은 VAO를 생성한다.
        static GLuint CreateVao(const GLuint* buffers, const GLint* locs, const GLint* sizes, const GLenum* types,
            int attribCount, GLuint ibo);

        GLuint  posVbo = 0;
        GLuint	norVbo = 0;
        GLuint	uvVbo = 0;
        GLuint	ibo = 0;
    
    public:
        GlfwInstance();
        ~GlfwInstance() override;

        // OpenGL 버퍼와 VAO 리소스를 해제한다.
        void Clear() override;
        // 모델 데이터를 OpenGL 버퍼, VAO, 재질 리소스로 업로드한다.
        bool Setup(Viewer& baseViewer) override;
        // 모델의 갱신된 버텍스 데이터를 OpenGL 버퍼에 반영한다.
        void Update() const override;
    };
}
