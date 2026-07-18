#include "Viewer/DrawContext/Dx12DrawContext.h"

#include "Viewer/Command/Dx12CommandContext.h"
#include "Viewer/Pipeline/Dx12Pipeline.h"

namespace Chrivent {
	Dx12DrawContext::Dx12DrawContext(Dx12CommandContext& sourceCommandContext, Dx12Pipeline& sourcePipeline)
		: commandContext(sourceCommandContext), pipeline(sourcePipeline) {}

	void Dx12DrawContext::BeginFrame(const UINT sourceFrameIndex) {
		frameIndex = sourceFrameIndex;
		pipelineBinding = PipelineBinding::None;
		frameReady = true;
	}

	void Dx12DrawContext::EndFrame() {
		frameReady = false;
		pipelineBinding = PipelineBinding::None;
	}

	ID3D12GraphicsCommandList* Dx12DrawContext::TryGetCommandList() const {
		return frameReady ? commandContext.GetCommandList().Get() : nullptr;
	}

	void Dx12DrawContext::BindModelPipeline(const bool bothFace) {
		const PipelineBinding targetBinding = bothFace
			? PipelineBinding::ModelBothFace : PipelineBinding::ModelFrontFace;
		if (!frameReady || pipelineBinding == targetBinding)
			return;
		pipeline.BindModel(commandContext.GetCommandList().Get(), bothFace);
		pipelineBinding = targetBinding;
	}

	void Dx12DrawContext::BindDepthOnlyPipeline(const bool bothFace) {
		const PipelineBinding targetBinding = bothFace
			? PipelineBinding::DepthBothFace : PipelineBinding::DepthFrontFace;
		if (!frameReady || pipelineBinding == targetBinding)
			return;
		pipeline.BindDepthOnly(commandContext.GetCommandList().Get(), bothFace);
		pipelineBinding = targetBinding;
	}

	void Dx12DrawContext::BindSceneVelocityPipeline(const bool bothFace) {
		const PipelineBinding targetBinding = bothFace
			? PipelineBinding::VelocityBothFace : PipelineBinding::VelocityFrontFace;
		if (!frameReady || pipelineBinding == targetBinding)
			return;
		pipeline.BindSceneVelocity(commandContext.GetCommandList().Get(), bothFace);
		pipelineBinding = targetBinding;
	}

	void Dx12DrawContext::BindEdgePipeline() {
		if (!frameReady || pipelineBinding == PipelineBinding::Edge)
			return;
		pipeline.BindEdge(commandContext.GetCommandList().Get());
		pipelineBinding = PipelineBinding::Edge;
	}

	void Dx12DrawContext::BindGroundShadowPipeline() {
		if (!frameReady || pipelineBinding == PipelineBinding::GroundShadow)
			return;
		pipeline.BindGroundShadow(commandContext.GetCommandList().Get());
		pipelineBinding = PipelineBinding::GroundShadow;
	}
}
