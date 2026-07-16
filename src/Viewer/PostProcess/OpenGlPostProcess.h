#pragma once

#include "Viewer/Shader/OpenGlShader.h"
#include "Viewer/PostProcess/PostProcess.h"

#include <memory>
#include <vector>

namespace Chrivent {
	struct PostProcessFrameData;

	// 공통 실행 계획을 OpenGL framebuffer와 셰이더 프로그램으로 실행한다.
	class OpenGlPostProcess : public PostProcess {
		// OpenGL 후처리 리소스의 ping-pong framebuffer와 texture를 보관한다.
		struct Resource {
			GLuint framebuffers[2]{};
			GLuint textures[2]{};
		};

		GLuint sceneFramebuffer = 0;
		GLuint sceneColorMsaa = 0;
		GLuint resolveFramebuffer = 0;
		GLuint sceneColorTexture = 0;
		GLuint postProcessDepthFramebuffer = 0;
		GLuint postProcessDepthTexture = 0;
		GLuint postProcessVelocityTexture = 0;
		GLuint sceneDepthStencil = 0;
		GLuint postProcessVao = 0;
		GLuint frameDataBuffer = 0;
		GLuint parameterDataBuffer = 0;
		GLsizei postProcessSampleCount = 1;
		std::vector<Resource> resources;
		std::vector<std::unique_ptr<OpenGlPostProcessShader>> postProcessShaders;
		int targetWidth = 0;
		int targetHeight = 0;

		// 패키지가 선언한 transient/history texture와 framebuffer를 생성한다.
		bool CreateEffectResources();
		// 초기화가 필요한 모든 history texture를 0으로 지운다.
		void InitializeHistories();
		// pass 입력 경로에 대응하는 OpenGL texture를 반환한다.
		GLuint ResolveInputTexture(const PostProcessPassInputRoute& input) const;
		// pass 출력 경로에 대응하는 OpenGL framebuffer를 반환한다.
		GLuint ResolveOutputFramebuffer(const PostProcessPassRoute& route) const;
		// 패키지가 선언한 OpenGL effect texture를 해제한다.
		void ResetEffectResources();
		// 후처리용 화면 framebuffer 리소스를 해제한다.
		void ResetTargets();
		// 후처리 셰이더 프로그램만 해제한다.
		void ResetShaders();
		
	public:
		~OpenGlPostProcess() override;

		GLuint GetSceneFramebuffer() const { return HasEffects() ? sceneFramebuffer : 0; }

		// 화면 크기에 맞는 OpenGL 후처리용 화면 framebuffer를 생성한다.
		bool InitializeTargets(int width, int height, int sampleCount);
		// 체크된 후처리 effect 선언으로 OpenGL 실행 리소스와 shader chain을 생성한다.
		bool Load(const std::vector<const EffectRuntimeDefinition*>& effects);
		// OpenGL 후처리 장면 depth와 velocity 입력 패스를 시작한다.
		bool BeginSceneInputPass(int width, int height) const;
		// OpenGL 후처리 장면 입력 패스를 종료한다.
		static void EndSceneInputPass();
		// 준비된 실행 계획으로 화면 색상을 기본 framebuffer에 그린다.
		bool Draw(int width, int height, const PostProcessFrameData& frameData);
		// 생성한 OpenGL 후처리 리소스를 해제한다.
		void ResetResources() override;
	};
}
