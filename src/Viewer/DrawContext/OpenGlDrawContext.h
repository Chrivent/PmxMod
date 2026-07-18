#pragma once

#include "Viewer/Pipeline/OpenGlPipeline.h"

#include <glad/glad.h>

namespace Chrivent {
	// OpenGL Drawer와 Instance에 장면 패스 동작 및 입력 위치만 노출한다.
	class OpenGlDrawContext {
		GLuint dummyColorTexture = 0;
		const OpenGlPipeline& pipeline;

	public:
		explicit OpenGlDrawContext(const OpenGlPipeline& sourcePipeline) : pipeline(sourcePipeline) {}

		GLuint GetDummyColorTexture() const { return dummyColorTexture; }

		// 초기화된 기본 색상 텍스처를 Drawer에 제공하도록 저장한다.
		void SetDummyColorTexture(const GLuint texture) { dummyColorTexture = texture; }
		// 장면 패스별 vertex attribute 위치를 모델 VAO 생성용 값으로 반환한다.
		OpenGlSceneAttributeLocations ResolveSceneAttributeLocations() const;
		// 모델 표면 프로그램을 바인딩한다.
		void BindModelPipeline() const;
		// 엣지 프로그램을 바인딩한다.
		void BindEdgePipeline() const;
		// 지면 그림자 프로그램을 바인딩한다.
		void BindGroundShadowPipeline() const;
		// 장면 depth 프로그램을 바인딩한다.
		void BindDepthOnlyPipeline() const;
		// 장면 velocity 프로그램을 바인딩한다.
		void BindSceneVelocityPipeline() const;
	};
}
