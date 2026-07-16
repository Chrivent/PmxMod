#include "Viewer/DrawContext/Dx12DrawContext.h"

#include "Viewer/Command/Dx12CommandContext.h"
#include "Viewer/Pipeline/Dx12Pipeline.h"

namespace Chrivent {
	Dx12DrawContext::Dx12DrawContext(Dx12CommandContext& sourceCommandContext, Dx12Pipeline& sourcePipeline)
		: commandContext(sourceCommandContext), pipeline(sourcePipeline) {}

	void Dx12DrawContext::BeginFrame(const UINT sourceFrameIndex) {
		frameIndex = sourceFrameIndex;
		frameReady = true;
	}

	void Dx12DrawContext::EndFrame() {
		frameReady = false;
	}

	ID3D12GraphicsCommandList* Dx12DrawContext::ResolveCommandList() const {
		return frameReady ? commandContext.GetCommandList().Get() : nullptr;
	}

	void Dx12DrawContext::BindModelPipeline(const bool bothFace) const {
		if (frameReady)
			pipeline.BindModel(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12DrawContext::BindDepthOnlyPipeline(const bool bothFace) const {
		if (frameReady)
			pipeline.BindDepthOnly(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12DrawContext::BindSceneVelocityPipeline(const bool bothFace) const {
		if (frameReady)
			pipeline.BindSceneVelocity(commandContext.GetCommandList().Get(), bothFace);
	}

	void Dx12DrawContext::BindEdgePipeline() const {
		if (frameReady)
			pipeline.BindEdge(commandContext.GetCommandList().Get());
	}

	void Dx12DrawContext::BindGroundShadowPipeline() const {
		if (frameReady)
			pipeline.BindGroundShadow(commandContext.GetCommandList().Get());
	}
}
