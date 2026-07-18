#include "Viewer/Pipeline/OpenGlPipeline.h"

#include <iostream>

namespace Chrivent {
	bool OpenGlPipeline::Initialize(const BuiltInShaderPasses& builtInPasses,
		const SceneInputShaderPasses& sceneInputPasses) {
		if (!modelShader.Initialize(builtInPasses.model)) {
			std::cerr << "기본 OpenGL 셰이더를 설정하지 못했습니다.\n";
			return false;
		}
		if (!edgeShader.Initialize(builtInPasses.edge)) {
			std::cerr << "엣지 OpenGL 셰이더를 설정하지 못했습니다.\n";
			return false;
		}
		if (!groundShadowShader.Initialize(builtInPasses.groundShadow)) {
			std::cerr << "지면 그림자 OpenGL 셰이더를 설정하지 못했습니다.\n";
			return false;
		}
		if (!depthOnlyShader.Initialize(sceneInputPasses.depth)) {
			std::cerr << "depth-only OpenGL 셰이더를 설정하지 못했습니다.\n";
			return false;
		}
		if (!sceneVelocityShader.Initialize(sceneInputPasses.velocity)) {
			std::cerr << "장면 velocity OpenGL 셰이더를 설정하지 못했습니다.\n";
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
