#pragma once

#include "Viewer/Pipeline/OpenGlPipeline.h"

#include <glad/glad.h>

namespace Chrivent {
	// OpenGL Drawer와 Instance에 장면 그리기용 셰이더 및 기본 텍스처만 노출한다.
	class OpenGlDrawContext {
		GLuint dummyColorTexture = 0;
		const OpenGlPipeline& pipeline;

	public:
		explicit OpenGlDrawContext(const OpenGlPipeline& sourcePipeline) : pipeline(sourcePipeline) {}

		GLuint GetDummyColorTexture() const { return dummyColorTexture; }
		const OpenGlModelShader& GetModelShader() const { return pipeline.GetModelShader(); }
		const OpenGlEdgeShader& GetEdgeShader() const { return pipeline.GetEdgeShader(); }
		const OpenGlGroundShadowShader& GetGroundShadowShader() const { return pipeline.GetGroundShadowShader(); }
		const OpenGlDepthOnlyShader& GetDepthOnlyShader() const { return pipeline.GetDepthOnlyShader(); }
		const OpenGlSceneVelocityShader& GetSceneVelocityShader() const { return pipeline.GetSceneVelocityShader(); }

		// 초기화된 기본 색상 텍스처를 Drawer에 제공하도록 저장한다.
		void SetDummyColorTexture(const GLuint texture) { dummyColorTexture = texture; }
	};
}
