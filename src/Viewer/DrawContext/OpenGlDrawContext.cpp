#include "Viewer/DrawContext/OpenGlDrawContext.h"

namespace Chrivent {
	OpenGlSceneAttributeLocations OpenGlDrawContext::ResolveSceneAttributeLocations() const {
		const OpenGlModelShader& model = pipeline.GetModelShader();
		const OpenGlEdgeShader& edge = pipeline.GetEdgeShader();
		const OpenGlGroundShadowShader& groundShadow = pipeline.GetGroundShadowShader();
		const OpenGlDepthOnlyShader& depth = pipeline.GetDepthOnlyShader();
		const OpenGlSceneVelocityShader& velocity = pipeline.GetSceneVelocityShader();
		return {
			.model = { model.positionLocation, model.normalLocation, model.uvLocation },
			.edge = { edge.positionLocation, edge.normalLocation },
			.groundShadow = { groundShadow.positionLocation },
			.depth = { depth.positionLocation, depth.uvLocation },
			.velocity = { velocity.positionLocation, velocity.previousPositionLocation, velocity.uvLocation }
		};
	}

	void OpenGlDrawContext::BindModelPipeline() const {
		glUseProgram(pipeline.GetModelShader().program);
	}

	void OpenGlDrawContext::BindEdgePipeline() const {
		glUseProgram(pipeline.GetEdgeShader().program);
	}

	void OpenGlDrawContext::BindGroundShadowPipeline() const {
		glUseProgram(pipeline.GetGroundShadowShader().program);
	}

	void OpenGlDrawContext::BindDepthOnlyPipeline() const {
		glUseProgram(pipeline.GetDepthOnlyShader().program);
	}

	void OpenGlDrawContext::BindSceneVelocityPipeline() const {
		glUseProgram(pipeline.GetSceneVelocityShader().program);
	}
}
