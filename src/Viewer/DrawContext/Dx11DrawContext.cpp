#include "Viewer/DrawContext/Dx11DrawContext.h"

namespace Chrivent {
	ID3D11RasterizerState* Dx11DrawContext::ResolveModelRasterizerState(const bool bothFace) const {
		return bothFace ? pipeline.GetStates().bothFaceRs.Get() : pipeline.GetStates().frontFaceRs.Get();
	}

	void Dx11DrawContext::BindModelPipeline() const {
		ID3D11DeviceContext* context = device.GetContext();
		const Dx11ShaderSet& shaders = pipeline.GetShaders();
		const Dx11PipelineStates& states = pipeline.GetStates();
		context->OMSetDepthStencilState(states.defaultDss.Get(), 0x00);
		context->OMSetBlendState(states.blendState.Get(), nullptr, 0xffffffff);
		context->IASetInputLayout(shaders.model.inputLayout.Get());
		context->VSSetShader(shaders.model.vertexShader.Get(), nullptr, 0);
		context->PSSetShader(shaders.model.pixelShader.Get(), nullptr, 0);
	}

	void Dx11DrawContext::BindEdgePipeline() const {
		ID3D11DeviceContext* context = device.GetContext();
		const Dx11ShaderSet& shaders = pipeline.GetShaders();
		const Dx11PipelineStates& states = pipeline.GetStates();
		context->IASetInputLayout(shaders.edge.inputLayout.Get());
		context->VSSetShader(shaders.edge.vertexShader.Get(), nullptr, 0);
		context->PSSetShader(shaders.edge.pixelShader.Get(), nullptr, 0);
		context->RSSetState(states.edgeRs.Get());
		context->OMSetDepthStencilState(states.defaultDss.Get(), 0x00);
		context->OMSetBlendState(states.blendState.Get(), nullptr, 0xffffffff);
	}

	void Dx11DrawContext::BindGroundShadowPipeline() const {
		ID3D11DeviceContext* context = device.GetContext();
		const Dx11ShaderSet& shaders = pipeline.GetShaders();
		const Dx11PipelineStates& states = pipeline.GetStates();
		context->IASetInputLayout(shaders.groundShadow.inputLayout.Get());
		context->VSSetShader(shaders.groundShadow.vertexShader.Get(), nullptr, 0);
		context->PSSetShader(shaders.groundShadow.pixelShader.Get(), nullptr, 0);
		context->RSSetState(states.gsRs.Get());
		context->OMSetDepthStencilState(states.gsDss.Get(), 0x01);
		context->OMSetBlendState(states.groundShadowBlendState.Get(), nullptr, 0xffffffff);
	}

	void Dx11DrawContext::BindSceneInputPipeline(const bool velocity) const {
		ID3D11DeviceContext* context = device.GetContext();
		const Dx11ShaderSet& shaders = pipeline.GetShaders();
		if (velocity) {
			context->IASetInputLayout(shaders.sceneVelocity.inputLayout.Get());
			context->VSSetShader(shaders.sceneVelocity.vertexShader.Get(), nullptr, 0);
			context->PSSetShader(shaders.sceneVelocity.pixelShader.Get(), nullptr, 0);
		} else {
			context->IASetInputLayout(shaders.sceneDepth.inputLayout.Get());
			context->VSSetShader(shaders.sceneDepth.vertexShader.Get(), nullptr, 0);
			context->PSSetShader(shaders.sceneDepth.pixelShader.Get(), nullptr, 0);
		}
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
