#pragma once

#include "../Viewer.h"
#include "GlfwTextureCache.h"

namespace Chrivent {
    class GlfwViewer;

    struct GlfwShader {
        GLuint  program = 0;
        GLint   positionLocation = -1;
        GLint   wvpLocation = -1;

        virtual ~GlfwShader();
    };

    struct GlfwModelShader : GlfwShader {
        GLint   normalLocation = -1;
        GLint   uvLocation = -1;
        GLint   wvLocation = -1;
        GLint   ambientLocation = -1;
        GLint   diffuseLocation = -1;
        GLint   specularLocation = -1;
        GLint   specularPowerLocation = -1;
        GLint   alphaLocation = -1;
        GLint   texModeLocation = -1;
        GLint   texLocation = -1;
        GLint   texMulFactorLocation = -1;
        GLint   texAddFactorLocation = -1;
        GLint   sphereTexModeLocation = -1;
        GLint   sphereTexLocation = -1;
        GLint   sphereTexMulFactorLocation = -1;
        GLint   sphereTexAddFactorLocation = -1;
        GLint   toonTexModeLocation = -1;
        GLint   toonTexLocation = -1;
        GLint   toonTexMulFactorLocation = -1;
        GLint   toonTexAddFactorLocation = -1;
        GLint   lightColorLocation = -1;
        GLint   lightDirLocation = -1;

        // 모델 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
        bool Setup(const ViewerInfo& viewerInfo);
    };

    struct GlfwEdgeShader : GlfwShader {
        GLint   normalLocation = -1;
        GLint   wvLocation = -1;
        GLint   screenSizeLocation = -1;
        GLint   edgeSizeLocation = -1;
        GLint   edgeColorLocation = -1;

        // 엣지 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
        bool Setup(const ViewerInfo& viewerInfo);
    };

    struct GlfwGroundShadowShader : GlfwShader {
        GLint   shadowColorLocation = -1;

        // 지면 그림자 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
        bool Setup(const ViewerInfo& viewerInfo);
    };

    struct GlfwViewerMaterial : ViewerMaterial {
        GLuint  texture = 0;
        bool    textureHasAlpha = false;
        GLuint  sphereTexture = 0;
        GLuint  toonTexture = 0;

        explicit GlfwViewerMaterial(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
    };

    struct GlfwViewerInfo : ViewerInfo {
        GLuint                                      dummyColorTex = 0;
        std::unique_ptr<GlfwModelShader>            shader;
        std::unique_ptr<GlfwEdgeShader>             edgeShader;
        std::unique_ptr<GlfwGroundShadowShader>     gsShader;
    };

    class GlfwViewer : public Viewer {
        static void* LoadGlProc(const char* name) {
            return reinterpret_cast<void*>(glfwGetProcAddress(name));
        }

        const int           msaaSamples = 4;
        GlfwTextureCache    textureCache;

    public:
        GlfwViewer();
        ~GlfwViewer() override;

        GlfwViewerInfo& GetGlfwInfo() { return static_cast<GlfwViewerInfo&>(GetInfo()); }
        const GlfwViewerInfo& GetGlfwInfo() const { return static_cast<const GlfwViewerInfo&>(GetInfo()); }

        // OpenGL 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
        void ConfigureGlfwHints() override;
        // OpenGL 컨텍스트와 셰이더, 기본 텍스처를 초기화한다.
        bool Setup() override;
        // 창 크기에 맞춰 OpenGL 뷰포트와 투영 행렬을 갱신한다.
        bool Resize() override;
        // 컬러/깊이 버퍼를 지우고 프레임 렌더링을 시작한다.
        void BeginFrame() override;
        // GLFW 버퍼를 교체하고 이벤트 처리를 진행한다.
        bool EndFrame() override;
        // OpenGL 모델 인스턴스를 생성한다.
        std::unique_ptr<Instance> CreateInstance() const override;

        // 텍스처를 캐시에서 찾거나 파일에서 로드해 OpenGL 텍스처로 반환한다.
        GlfwTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false);
    };
}
