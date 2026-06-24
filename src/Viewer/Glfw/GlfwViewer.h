#pragma once

#include "Viewer/Viewer.h"
#include "Viewer/Glfw/GlfwTextureCache.h"
#include "Viewer/Glfw/Helper/GlfwShader.h"

#include <filesystem>
#include <memory>

namespace Chrivent {
    class GlfwViewer;

    struct GlfwMaterial : ViewerMaterial {
        GLuint  texture = 0;
        bool    textureHasAlpha = false;
        GLuint  sphereTexture = 0;
        GLuint  toonTexture = 0;

        explicit GlfwMaterial(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
    };

    class GlfwViewer : public Viewer {
        // GLAD가 사용할 OpenGL 함수 포인터를 GLFW에서 조회한다.
        static void* LoadGlProc(const char* name) {
            return reinterpret_cast<void*>(glfwGetProcAddress(name));
        }
        // OpenGL renderer 이름을 GPU 종류 이름으로 분류한다.
        static const char* ResolveGpuTypeName(const std::string& renderer);

        const int           msaaSamples = 4;
        GlfwTextureCache    textureCache;
        GLuint sceneFramebuffer = 0;
        GLuint sceneColorMsaa = 0;
        GLuint resolveFramebuffer = 0;
        GLuint sceneColorTexture = 0;
        GLuint sceneDepthStencil = 0;
        GLuint postProcessVao = 0;
        GLsizei postProcessSampleCount = 1;
        std::unique_ptr<GlfwPostProcessShader> postProcessShader;

        // 화면 크기에 맞는 후처리용 장면 프레임버퍼를 생성한다.
        bool CreatePostProcessTargets();
        // 후처리용 장면 프레임버퍼 리소스를 해제한다.
        void ResetPostProcessTargets();

    public:
        GLuint dummyColorTex = 0;
        std::unique_ptr<GlfwModelShader> shader;
        std::unique_ptr<GlfwEdgeShader> edgeShader;
        std::unique_ptr<GlfwGroundShadowShader> gsShader;

        GlfwViewer() = default;
        ~GlfwViewer() override;

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
        // OpenGL 명령이 모두 처리될 때까지 기다린다.
        void WaitIdle() override;
        // 선택한 후처리 HLSL을 OpenGL 프로그램으로 준비한다.
        bool LoadPostProcessEffect(const EffectDefinition& effect) override;
        // OpenGL 후처리 프로그램을 해제한다.
        void ClearPostProcessEffect() override;
        // OpenGL 모델 인스턴스를 생성한다.
        std::unique_ptr<Instance> CreateInstance() const override;

        // 텍스처를 캐시에서 찾거나 파일에서 로드해 OpenGL 텍스처로 반환한다.
        GlfwTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false) {
            return textureCache.Load(texturePath, clamp);
        }
    };
}
