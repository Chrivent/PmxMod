#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/DrawContext/OpenGlDrawContext.h"
#include "Viewer/PostProcess/OpenGlPostProcess.h"
#include "Viewer/Texture/OpenGlTextureCache.h"

#include <filesystem>
#include <memory>

namespace Chrivent {
    // 공통 Viewer 계약을 OpenGL 컨텍스트와 framebuffer 흐름으로 구현한다.
    class OpenGlViewer : public Viewer {
        // GLAD가 사용할 OpenGL 함수 포인터를 GLFW에서 조회한다.
        static void* LoadGlProc(const char* name) {
            return reinterpret_cast<void*>(glfwGetProcAddress(name));
        }
        // OpenGL renderer 이름을 GPU 종류 이름으로 분류한다.
        static const char* ResolveGpuTypeName(const std::string& renderer);

        const int           msaaSamples = 4;
        OpenGlTextureCache    textureCache;
        OpenGlPostProcess postProcess;
        GLuint dummyColorTex = 0;
        std::unique_ptr<OpenGlModelShader> shader;
        std::unique_ptr<OpenGlEdgeShader> edgeShader;
        std::unique_ptr<OpenGlGroundShadowShader> gsShader;
        std::unique_ptr<OpenGlDepthOnlyShader> depthOnlyShader;
        std::unique_ptr<OpenGlSceneVelocityShader> sceneVelocityShader;
		OpenGlDrawContext drawContext{ dummyColorTex, shader, edgeShader, gsShader,
			depthOnlyShader, sceneVelocityShader };

    protected:
        PostProcess& ResolvePostProcess() override { return postProcess; }
        const PostProcess& ResolvePostProcess() const override { return postProcess; }
        
        // 체크된 후처리 HLSL들을 OpenGL ping-pong 체인으로 준비한다.
        bool LoadPostProcessEffectsCore(const std::vector<const EffectDefinition*>& effects) override;
		// OpenGL 후처리 장면 입력 패스 기록을 시작한다.
		bool BeginPostProcessSceneInputPassCore() override;
        // 초기 상태의 OpenGL 모델 인스턴스를 생성한다.
        std::unique_ptr<Instance> CreateInstanceCore() override;

    public:
        ~OpenGlViewer() override;

		const OpenGlDrawContext& GetDrawContext() const { return drawContext; }

        // OpenGL 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
        void ConfigureWindowHints() override;
        // OpenGL 컨텍스트와 셰이더, 기본 텍스처를 초기화한다.
        bool Setup() override;
        // 창 크기에 맞춰 OpenGL 뷰포트와 투영 행렬을 갱신한다.
        bool Resize() override;
        // 컬러/깊이 버퍼를 지우고 프레임 렌더링을 시작한다.
        FrameBeginResult BeginFrame() override;
        // GLFW 버퍼를 교체하고 이벤트 처리를 진행한다.
        FrameEndResult EndFrame() override;
        // OpenGL 후처리 장면 입력 패스를 종료한다.
        bool EndPostProcessSceneInputPass() override;
        // OpenGL 명령이 모두 처리될 때까지 기다린다.
        bool WaitIdle() override;
        // 텍스처를 캐시에서 찾거나 파일에서 로드해 OpenGL 텍스처로 반환한다.
        OpenGlTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false) {
            return textureCache.Load(texturePath, clamp);
        }
    };
}
