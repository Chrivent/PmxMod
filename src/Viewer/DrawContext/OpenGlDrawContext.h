#pragma once

#include "Viewer/Shader/OpenGlShader.h"

#include <glad/glad.h>

#include <memory>

namespace Chrivent {
	// OpenGL Drawer와 Instance에 장면 그리기용 셰이더 및 기본 텍스처만 노출한다.
	class OpenGlDrawContext {
		const GLuint& dummyColorTexture;
		const std::unique_ptr<OpenGlModelShader>& modelShader;
		const std::unique_ptr<OpenGlEdgeShader>& edgeShader;
		const std::unique_ptr<OpenGlGroundShadowShader>& groundShadowShader;
		const std::unique_ptr<OpenGlDepthOnlyShader>& depthOnlyShader;
		const std::unique_ptr<OpenGlSceneVelocityShader>& sceneVelocityShader;

	public:
		OpenGlDrawContext(const GLuint& sourceDummyColorTexture,
			const std::unique_ptr<OpenGlModelShader>& sourceModelShader,
			const std::unique_ptr<OpenGlEdgeShader>& sourceEdgeShader,
			const std::unique_ptr<OpenGlGroundShadowShader>& sourceGroundShadowShader,
			const std::unique_ptr<OpenGlDepthOnlyShader>& sourceDepthOnlyShader,
			const std::unique_ptr<OpenGlSceneVelocityShader>& sourceSceneVelocityShader)
			: dummyColorTexture(sourceDummyColorTexture), modelShader(sourceModelShader),
			edgeShader(sourceEdgeShader), groundShadowShader(sourceGroundShadowShader),
			depthOnlyShader(sourceDepthOnlyShader), sceneVelocityShader(sourceSceneVelocityShader) {}

		GLuint GetDummyColorTexture() const { return dummyColorTexture; }
		const OpenGlModelShader* GetModelShader() const { return modelShader.get(); }
		const OpenGlEdgeShader* GetEdgeShader() const { return edgeShader.get(); }
		const OpenGlGroundShadowShader* GetGroundShadowShader() const { return groundShadowShader.get(); }
		const OpenGlDepthOnlyShader* GetDepthOnlyShader() const { return depthOnlyShader.get(); }
		const OpenGlSceneVelocityShader* GetSceneVelocityShader() const { return sceneVelocityShader.get(); }
	};
}
