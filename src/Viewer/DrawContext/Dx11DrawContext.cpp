#include "Viewer/DrawContext/Dx11DrawContext.h"

namespace Chrivent {
	ID3D11RasterizerState* Dx11DrawContext::ResolveModelRasterizerState(const bool bothFace) const {
		return pipeline.ResolveModelRasterizerState(bothFace);
	}

	void Dx11DrawContext::BindModelPipeline() const {
		pipeline.BindModel(device.GetContext());
	}

	void Dx11DrawContext::BindEdgePipeline() const {
		pipeline.BindEdge(device.GetContext());
	}

	void Dx11DrawContext::BindGroundShadowPipeline() const {
		pipeline.BindGroundShadow(device.GetContext());
	}

	void Dx11DrawContext::BindSceneInputPipeline(const bool velocity) const {
		pipeline.BindSceneInput(device.GetContext(), velocity);
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
