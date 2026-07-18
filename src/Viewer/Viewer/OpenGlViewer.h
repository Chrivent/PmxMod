#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/DrawContext/OpenGlDrawContext.h"
#include "Viewer/Pipeline/OpenGlPipeline.h"
#include "Viewer/PostProcess/OpenGlPostProcess.h"
#include "Viewer/Texture/OpenGlTextureCache.h"

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
		OpenGlPipeline pipeline;
		OpenGlDrawContext drawContext{ pipeline };

    protected:
        // 체크된 후처리 HLSL들을 OpenGL ping-pong 체인으로 준비한다.
        bool LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) override;
		// OpenGL 후처리 장면 입력 패스 기록을 시작한다.
		bool BeginPostProcessSceneInputPassCore() override;
		// OpenGL 후처리 장면 입력 패스를 종료한다.
		bool EndPostProcessSceneInputPassCore() override;
		// OpenGL 컨텍스트와 셰이더, 기본 텍스처를 초기화한다.
		bool SetupCore() override;
		// 창 크기에 맞춰 OpenGL 뷰포트와 렌더 타깃을 갱신한다.
		bool ResizeCore() override;
		// 컬러와 깊이 버퍼를 지우고 OpenGL 프레임을 시작한다.
		FrameBeginResult BeginFrameCore() override;
        // OpenGL 버퍼 교체 결과를 반환한다.
        FrameEndResult EndFrameCore() override;
        // 초기 상태의 OpenGL 모델 인스턴스를 생성한다.
        std::unique_ptr<Instance> CreateInstanceCore() override;

    public:
        ~OpenGlViewer() override;

		bool IsVelocityYInverted() const override { return false; }

        // OpenGL 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
        void ConfigureWindowHints() override;
        // OpenGL 명령이 모두 처리될 때까지 기다린다.
        bool WaitIdle() override;
    };
}
