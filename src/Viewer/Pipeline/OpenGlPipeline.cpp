#include "Viewer/Pipeline/OpenGlPipeline.h"

namespace Chrivent {
	GraphicsResult<void> OpenGlPipeline::Initialize(const SceneShaderRuntimeContract& shaderContract) {
		const auto& [model, edge, groundShadow] = shaderContract.builtIn;
		const auto& [depth, velocity] = shaderContract.sceneInput;
		std::string error;
		if (!modelShader.Initialize(model, error))
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "모델 셰이더 생성",
				error.empty() ? "기본 OpenGL 셰이더를 설정하지 못했습니다" : error));
		if (!edgeShader.Initialize(edge, error))
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "엣지 셰이더 생성",
				error.empty() ? "엣지 OpenGL 셰이더를 설정하지 못했습니다" : error));
		if (!groundShadowShader.Initialize(groundShadow, error))
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "지면 그림자 셰이더 생성",
				error.empty() ? "지면 그림자 OpenGL 셰이더를 설정하지 못했습니다" : error));
		if (!depthOnlyShader.Initialize(depth, error))
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "장면 depth 셰이더 생성",
				error.empty() ? "depth-only OpenGL 셰이더를 설정하지 못했습니다" : error));
		if (!sceneVelocityShader.Initialize(velocity, error))
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "장면 velocity 셰이더 생성",
				error.empty() ? "장면 velocity OpenGL 셰이더를 설정하지 못했습니다" : error));
		return {};
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
