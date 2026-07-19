#pragma once

#include "Viewer/Pipeline/OpenGlPipeline.h"

#include <glad/glad.h>

namespace Chrivent {
	// OpenGL Drawer와 Instance에 장면 패스 동작 및 입력 위치만 노출한다.
	class OpenGlDrawContext {
		enum class BoundPipeline {
			None,
			Model,
			Edge,
			GroundShadow,
			DepthOnly,
			SceneVelocity
		};

		GLuint dummyColorTexture = 0;
		const OpenGlPipeline& pipeline;
		BoundPipeline boundPipeline = BoundPipeline::None;

	public:
		explicit OpenGlDrawContext(const OpenGlPipeline& sourcePipeline) : pipeline(sourcePipeline) {}

		GLuint GetDummyColorTexture() const { return dummyColorTexture; }

		// 새 프레임에서 외부 패스가 바꿀 수 있는 프로그램 바인딩 캐시를 초기화한다.
		void BeginFrame() { boundPipeline = BoundPipeline::None; }
		// 초기화된 기본 색상 텍스처를 Drawer에 제공하도록 저장한다.
		void SetDummyColorTexture(const GLuint texture) { dummyColorTexture = texture; }
		// 모델 표면 프로그램을 바인딩한다.
		void BindModelPipeline();
		// 엣지 프로그램을 바인딩한다.
		void BindEdgePipeline();
		// 지면 그림자 프로그램을 바인딩한다.
		void BindGroundShadowPipeline();
		// 장면 depth 프로그램을 바인딩한다.
		void BindDepthOnlyPipeline();
		// 장면 velocity 프로그램을 바인딩한다.
		void BindSceneVelocityPipeline();
	};
}
