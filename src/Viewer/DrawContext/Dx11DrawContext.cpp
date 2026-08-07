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

	void Dx11DrawContext::BindSceneDepthPipeline() {
		if (boundPipeline == BoundPipeline::SceneDepth)
			return;
		pipeline.BindSceneDepth(device.GetContext());
		boundPipeline = BoundPipeline::SceneDepth;
	}

	void Dx11DrawContext::BindSceneVelocityPipeline() {
		if (boundPipeline == BoundPipeline::SceneVelocity)
			return;
		pipeline.BindSceneVelocity(device.GetContext());
		boundPipeline = BoundPipeline::SceneVelocity;
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

	GraphicsError::Result<void> Dx11DrawContext::WriteConstantBuffer(ID3D11DeviceContext* context,
		ID3D11Buffer* buffer, const void* data, const size_t size, const char* operation) {
		if (context == nullptr || buffer == nullptr || data == nullptr || size == 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, operation,
				"DirectX 11 상수 버퍼 입력이 올바르지 않습니다"));
		}
		D3D11_MAPPED_SUBRESOURCE mappedResource{};
		const HRESULT result = context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (FAILED(result)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX11,
				GraphicsErrorCode::CommandRecordingFailed, operation,
				"DirectX 11 상수 버퍼를 매핑하지 못했습니다", result, true));
		}
		std::memcpy(mappedResource.pData, data, size);
		context->Unmap(buffer, 0);
		return {};
	}
}
