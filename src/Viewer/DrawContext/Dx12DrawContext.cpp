#include "Viewer/DrawContext/Dx12DrawContext.h"

#include "Viewer/Command/Dx12CommandContext.h"
#include "Viewer/Pipeline/Dx12Pipeline.h"

namespace Chrivent {
	Dx12DrawContext::Dx12DrawContext(Dx12CommandContext& sourceCommandContext, Dx12Pipeline& sourcePipeline)
		: commandContext(sourceCommandContext), pipeline(sourcePipeline) {}

	void Dx12DrawContext::BeginFrame(const UINT sourceFrameIndex) {
		frameIndex = sourceFrameIndex;
		rootSignatureBinding = RootSignatureBinding::None;
		pipelineBinding = PipelineBinding::None;
		frameReady = true;
	}

	void Dx12DrawContext::EndFrame() {
		frameReady = false;
		rootSignatureBinding = RootSignatureBinding::None;
		pipelineBinding = PipelineBinding::None;
	}

	ID3D12GraphicsCommandList* Dx12DrawContext::TryGetCommandList() const {
		return frameReady ? commandContext.TryGetCommandList() : nullptr;
	}

	void Dx12DrawContext::BindModelRootSignature() {
		if (!frameReady || rootSignatureBinding == RootSignatureBinding::Model)
			return;
		pipeline.BindModelRootSignature(commandContext.TryGetCommandList());
		rootSignatureBinding = RootSignatureBinding::Model;
		pipelineBinding = PipelineBinding::None;
	}

	void Dx12DrawContext::BindModelPipeline(const bool bothFace) {
		BindModelRootSignature();
		const PipelineBinding targetBinding = bothFace
			? PipelineBinding::ModelBothFace : PipelineBinding::ModelFrontFace;
		if (!frameReady || pipelineBinding == targetBinding)
			return;
		pipeline.BindModelPipelineState(commandContext.TryGetCommandList(), bothFace);
		pipelineBinding = targetBinding;
	}

	void Dx12DrawContext::BindSceneDepthPipeline(const bool bothFace) {
		BindModelRootSignature();
		const PipelineBinding targetBinding = bothFace
			? PipelineBinding::SceneDepthBothFace : PipelineBinding::SceneDepthFrontFace;
		if (!frameReady || pipelineBinding == targetBinding)
			return;
		pipeline.BindSceneDepthPipelineState(commandContext.TryGetCommandList(), bothFace);
		pipelineBinding = targetBinding;
	}

	void Dx12DrawContext::BindSceneVelocityPipeline(const bool bothFace) {
		BindModelRootSignature();
		const PipelineBinding targetBinding = bothFace
			? PipelineBinding::VelocityBothFace : PipelineBinding::VelocityFrontFace;
		if (!frameReady || pipelineBinding == targetBinding)
			return;
		pipeline.BindSceneVelocityPipelineState(commandContext.TryGetCommandList(), bothFace);
		pipelineBinding = targetBinding;
	}

	void Dx12DrawContext::BindEdgePipeline() {
		if (!frameReady || pipelineBinding == PipelineBinding::Edge)
			return;
		pipeline.BindEdge(commandContext.TryGetCommandList());
		rootSignatureBinding = RootSignatureBinding::Edge;
		pipelineBinding = PipelineBinding::Edge;
	}

	void Dx12DrawContext::BindGroundShadowPipeline() {
		if (!frameReady || pipelineBinding == PipelineBinding::GroundShadow)
			return;
		pipeline.BindGroundShadow(commandContext.TryGetCommandList());
		rootSignatureBinding = RootSignatureBinding::GroundShadow;
		pipelineBinding = PipelineBinding::GroundShadow;
	}
}
