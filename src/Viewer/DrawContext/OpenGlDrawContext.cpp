#include "Viewer/DrawContext/OpenGlDrawContext.h"

namespace Chrivent {
	OpenGlSceneAttributeLocations OpenGlDrawContext::ResolveSceneAttributeLocations() const {
		return pipeline.ResolveSceneAttributeLocations();
	}

	void OpenGlDrawContext::BindModelPipeline() const {
		pipeline.BindModel();
	}

	void OpenGlDrawContext::BindEdgePipeline() const {
		pipeline.BindEdge();
	}

	void OpenGlDrawContext::BindGroundShadowPipeline() const {
		pipeline.BindGroundShadow();
	}

	void OpenGlDrawContext::BindDepthOnlyPipeline() const {
		pipeline.BindDepthOnly();
	}

	void OpenGlDrawContext::BindSceneVelocityPipeline() const {
		pipeline.BindSceneVelocity();
	}
}
