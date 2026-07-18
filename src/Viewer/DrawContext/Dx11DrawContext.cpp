#include "Viewer/DrawContext/Dx11DrawContext.h"

namespace Chrivent {
	ID3D11RasterizerState* Dx11DrawContext::ResolveModelRasterizerState(const bool bothFace) const {
		return pipeline.ResolveModelRasterizerState(bothFace);
	}

	void Dx11DrawContext::BindModelPipeline() {
		if (boundPipeline == BoundPipeline::Model)
			return;
		pipeline.BindModel(device.GetContext());
		boundPipeline = BoundPipeline::Model;
	}

	void Dx11DrawContext::BindEdgePipeline() {
		if (boundPipeline == BoundPipeline::Edge)
			return;
		pipeline.BindEdge(device.GetContext());
		boundPipeline = BoundPipeline::Edge;
	}

	void Dx11DrawContext::BindGroundShadowPipeline() {
		if (boundPipeline == BoundPipeline::GroundShadow)
			return;
		pipeline.BindGroundShadow(device.GetContext());
		boundPipeline = BoundPipeline::GroundShadow;
	}

	void Dx11DrawContext::BindSceneInputPipeline(const bool velocity) {
		const BoundPipeline target = velocity ? BoundPipeline::SceneVelocity : BoundPipeline::DepthOnly;
		if (boundPipeline == target)
			return;
		pipeline.BindSceneInput(device.GetContext(), velocity);
		boundPipeline = target;
	}

	void Dx11DrawContext::ApplyViewport(ID3D11DeviceContext* context, const int width, const int height) {
		if (context == nullptr)
			return;
		D3D11_VIEWPORT viewport{};
		viewport.Width = static_cast<float>(width);
		viewport.Height = static_cast<float>(height);
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		context->RSSetViewports(1, &viewport);
	}
}
