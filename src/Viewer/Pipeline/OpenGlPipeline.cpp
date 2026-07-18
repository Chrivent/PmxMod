#include "Viewer/Pipeline/OpenGlPipeline.h"

#include <iostream>

namespace Chrivent {
	bool OpenGlPipeline::Initialize(const BuiltInShaderPasses& builtInPasses,
		const SceneInputShaderPasses& sceneInputPasses) {
		if (!modelShader.Initialize(builtInPasses.model)) {
			std::cerr << "Failed to set up main OpenGL shader.\n";
			return false;
		}
		if (!edgeShader.Initialize(builtInPasses.edge)) {
			std::cerr << "Failed to set up edge OpenGL shader.\n";
			return false;
		}
		if (!groundShadowShader.Initialize(builtInPasses.groundShadow)) {
			std::cerr << "Failed to set up ground shadow OpenGL shader.\n";
			return false;
		}
		if (!depthOnlyShader.Initialize(sceneInputPasses.depth)) {
			std::cerr << "Failed to set up depth-only OpenGL shader.\n";
			return false;
		}
		if (!sceneVelocityShader.Initialize(sceneInputPasses.velocity)) {
			std::cerr << "Failed to set up scene velocity OpenGL shader.\n";
			return false;
		}
		return true;
	}

	OpenGlSceneAttributeLocations OpenGlPipeline::ResolveSceneAttributeLocations() const {
		return {
			.model = { modelShader.positionLocation, modelShader.normalLocation, modelShader.uvLocation },
			.edge = { edgeShader.positionLocation, edgeShader.normalLocation },
			.groundShadow = { groundShadowShader.positionLocation },
			.depth = { depthOnlyShader.positionLocation, depthOnlyShader.uvLocation },
			.velocity = {
				sceneVelocityShader.positionLocation,
				sceneVelocityShader.previousPositionLocation,
				sceneVelocityShader.uvLocation
			}
		};
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
