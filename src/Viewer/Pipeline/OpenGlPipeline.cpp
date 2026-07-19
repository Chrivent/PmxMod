#include "Viewer/Pipeline/OpenGlPipeline.h"

namespace Chrivent {
	GraphicsResult<void> OpenGlPipeline::Initialize(const SceneShaderRuntimeContract& shaderContract) {
		const auto& [model, edge, groundShadow] = shaderContract.builtIn;
		const auto& [depth, velocity] = shaderContract.sceneInput;
		auto result = modelShader.Initialize(model);
		if (result)
			result = edgeShader.Initialize(edge);
		if (result)
			result = groundShadowShader.Initialize(groundShadow);
		if (result)
			result = depthOnlyShader.Initialize(depth);
		if (result)
			result = sceneVelocityShader.Initialize(velocity);
		return result;
	}

	void OpenGlPipeline::BindModel() const {
		glUseProgram(modelShader.program);
	}

	void OpenGlPipeline::BindEdge() const {
		glUseProgram(edgeShader.program);
	}

	void OpenGlPipeline::BindGroundShadow() const {
		glUseProgram(groundShadowShader.program);
	}

	void OpenGlPipeline::BindDepthOnly() const {
		glUseProgram(depthOnlyShader.program);
	}

	void OpenGlPipeline::BindSceneVelocity() const {
		glUseProgram(sceneVelocityShader.program);
	}
}
