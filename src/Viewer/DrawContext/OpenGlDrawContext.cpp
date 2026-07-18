#include "Viewer/DrawContext/OpenGlDrawContext.h"

namespace Chrivent {
	OpenGlSceneAttributeLocations OpenGlDrawContext::ResolveSceneAttributeLocations() const {
		return pipeline.ResolveSceneAttributeLocations();
	}

	void OpenGlDrawContext::BindModelPipeline() {
		if (boundPipeline == BoundPipeline::Model)
			return;
		pipeline.BindModel();
		boundPipeline = BoundPipeline::Model;
	}

	void OpenGlDrawContext::BindEdgePipeline() {
		if (boundPipeline == BoundPipeline::Edge)
			return;
		pipeline.BindEdge();
		boundPipeline = BoundPipeline::Edge;
	}

	void OpenGlDrawContext::BindGroundShadowPipeline() {
		if (boundPipeline == BoundPipeline::GroundShadow)
			return;
		pipeline.BindGroundShadow();
		boundPipeline = BoundPipeline::GroundShadow;
	}

	void OpenGlDrawContext::BindDepthOnlyPipeline() {
		if (boundPipeline == BoundPipeline::DepthOnly)
			return;
		pipeline.BindDepthOnly();
		boundPipeline = BoundPipeline::DepthOnly;
	}

	void OpenGlDrawContext::BindSceneVelocityPipeline() {
		if (boundPipeline == BoundPipeline::SceneVelocity)
			return;
		pipeline.BindSceneVelocity();
		boundPipeline = BoundPipeline::SceneVelocity;
	}
}
