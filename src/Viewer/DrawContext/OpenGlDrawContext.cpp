#include "Viewer/DrawContext/OpenGlDrawContext.h"

namespace Chrivent {
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

	void OpenGlDrawContext::BindSceneDepthPipeline() {
		if (boundPipeline == BoundPipeline::SceneDepth)
			return;
		pipeline.BindSceneDepth();
		boundPipeline = BoundPipeline::SceneDepth;
	}

	void OpenGlDrawContext::BindSceneVelocityPipeline() {
		if (boundPipeline == BoundPipeline::SceneVelocity)
			return;
		pipeline.BindSceneVelocity();
		boundPipeline = BoundPipeline::SceneVelocity;
	}
}
