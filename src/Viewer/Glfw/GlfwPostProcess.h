#pragma once

#include "Viewer/Glfw/Helper/GlfwShader.h"
#include "Viewer/PostProcess.h"

#include <memory>
#include <vector>

namespace Chrivent {
	struct PostProcessFrameData;

	class GlfwPostProcess : public PostProcess {
		GLuint sceneFramebuffer = 0;
		GLuint sceneColorMsaa = 0;
		GLuint resolveFramebuffer = 0;
		GLuint sceneColorTexture = 0;
		GLuint postProcessDepthFramebuffer = 0;
		GLuint postProcessDepthTexture = 0;
		GLuint pingPongFramebuffers[2] = {};
		GLuint pingPongTextures[2] = {};
		GLuint focusHistoryFramebuffers[2] = {};
		GLuint focusHistoryTextures[2] = {};
		GLuint sceneDepthStencil = 0;
		GLuint postProcessVao = 0;
		GLuint frameDataBuffer = 0;
		GLsizei postProcessSampleCount = 1;
		std::unique_ptr<GlfwPostProcessShader> focusHistoryShader;
		std::vector<std::unique_ptr<GlfwPostProcessShader>> postProcessShaders;
		size_t focusHistoryIndex = 0;
		bool focusHistoryInitialized = false;

		// 초기화 요청 뒤 초점 히스토리 target들을 0으로 지운다.
		void InitializeFocusHistory();
		// DOF용 자동 초점 히스토리 텍스처를 갱신한다.
		void UpdateFocusHistory();
		// 후처리용 화면 프레임버퍼 리소스를 해제한다.
		void ResetTargets();
		// 후처리 셰이더 프로그램만 해제한다.
		void ResetShaders();

	public:
		~GlfwPostProcess() override;

		GLuint ResolveSceneFramebuffer() const { return HasEffects() ? sceneFramebuffer : 0; }

		// 화면 크기에 맞는 OpenGL 후처리용 화면 프레임버퍼를 생성한다.
		bool InitializeTargets(int width, int height, int msaaSamples, uint32_t maxSampleCount);
		// 체크된 후처리 effect 목록으로 OpenGL fullscreen shader chain을 생성한다.
		bool Load(const std::vector<const EffectDefinition*>& effects);
		// OpenGL 후처리 프로그램과 선택 effect 목록만 해제한다.
		void ClearShaders();
		// OpenGL 포스트 프로세스용 단일 샘플 depth-only pass를 시작한다.
		bool BeginDepthPass(int width, int height) const;
		// OpenGL 포스트 프로세스용 단일 샘플 depth-only pass를 종료한다.
		static void EndDepthPass();
		// 준비된 후처리 프로그램으로 화면 색상을 기본 framebuffer에 그린다.
		void Draw(int width, int height, const PostProcessFrameData& frameData);
		// 다음 후처리 프레임에서 OpenGL 초점 히스토리를 0으로 초기화한다.
		void ResetHistory() override;
		// 생성한 OpenGL 후처리 리소스를 해제한다.
		void Reset() override;
	};
}
