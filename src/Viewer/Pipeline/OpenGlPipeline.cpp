#include "Viewer/Pipeline/OpenGlPipeline.h"

namespace Chrivent {
	GraphicsError::Result<void> OpenGlPipeline::Initialize(const SceneShaderRuntimeContract& shaderContract) {
		const auto& [model, edge, groundShadow] = shaderContract.builtIn;
		const auto& [depth, velocity] = shaderContract.sceneInput;
		OpenGlPipeline candidate;
		auto result = candidate.modelProgram.Initialize(model);
		if (result)
			result = candidate.edgeProgram.Initialize(edge);
		if (result)
			result = candidate.groundShadowProgram.Initialize(groundShadow);
		if (result)
			result = candidate.sceneDepthProgram.Initialize(depth);
		if (result)
			result = candidate.sceneVelocityProgram.Initialize(velocity);
		if (!result)
			return result;
		std::swap(modelProgram, candidate.modelProgram);
		std::swap(edgeProgram, candidate.edgeProgram);
		std::swap(groundShadowProgram, candidate.groundShadowProgram);
		std::swap(sceneDepthProgram, candidate.sceneDepthProgram);
		std::swap(sceneVelocityProgram, candidate.sceneVelocityProgram);
		return result;
	}

	void OpenGlPipeline::BindModel() const {
		glUseProgram(modelProgram.GetProgram());
	}

	void OpenGlPipeline::BindEdge() const {
		glUseProgram(edgeProgram.GetProgram());
	}

	void OpenGlPipeline::BindGroundShadow() const {
		glUseProgram(groundShadowProgram.GetProgram());
	}

	void OpenGlPipeline::BindSceneDepth() const {
		glUseProgram(sceneDepthProgram.GetProgram());
	}

	void OpenGlPipeline::BindSceneVelocity() const {
		glUseProgram(sceneVelocityProgram.GetProgram());
	}
}
