#include "Viewer/Pipeline/Dx11Pipeline.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"

namespace Chrivent {
	bool Dx11Pipeline::CreateShaders(ID3D11Device* device, const BuiltInShaderPasses& builtInPasses,
		const SceneInputShaderPasses& sceneInputPasses) {
		return shaders.model.Initialize(device, builtInPasses.model)
			&& shaders.edge.Initialize(device, builtInPasses.edge)
			&& shaders.groundShadow.Initialize(device, builtInPasses.groundShadow)
			&& shaders.sceneDepth.Initialize(device, sceneInputPasses.depth)
			&& shaders.sceneVelocity.Initialize(device, sceneInputPasses.velocity);
	}

	bool Dx11Pipeline::CreateStates(ID3D11Device* device) {
		auto wrapLinear = Dx11DescBuilder::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR,
			D3D11_TEXTURE_ADDRESS_WRAP);
		if (FAILED(device->CreateSamplerState(&wrapLinear, &states.textureSampler)))
			return false;
		auto clampLinear = Dx11DescBuilder::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR,
			D3D11_TEXTURE_ADDRESS_CLAMP);
		if (FAILED(device->CreateSamplerState(&clampLinear, &states.toonTextureSampler)))
			return false;
		auto blend = Dx11DescBuilder::MakeAlphaBlendDesc();
		if (FAILED(device->CreateBlendState(&blend, &states.blendState)))
			return false;
		auto groundShadowBlend = Dx11DescBuilder::MakeGroundShadowBlendDesc();
		if (FAILED(device->CreateBlendState(&groundShadowBlend, &states.groundShadowBlendState)))
			return false;
		auto frontDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_BACK, true);
		if (FAILED(device->CreateRasterizerState(&frontDescription, &states.frontFaceRs)))
			return false;
		auto bothDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		if (FAILED(device->CreateRasterizerState(&bothDescription, &states.bothFaceRs)))
			return false;
		auto edgeDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_FRONT, true);
		if (FAILED(device->CreateRasterizerState(&edgeDescription, &states.edgeRs)))
			return false;
		auto shadowDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		shadowDescription.DepthBias = -1;
		shadowDescription.SlopeScaledDepthBias = -1.0f;
		shadowDescription.DepthBiasClamp = -1.0f;
		if (FAILED(device->CreateRasterizerState(&shadowDescription, &states.gsRs)))
			return false;
		auto shadowDepthDescription = Dx11DescBuilder::MakeGroundShadowDepthStencilDesc();
		if (FAILED(device->CreateDepthStencilState(&shadowDepthDescription, &states.gsDss)))
			return false;
		auto defaultDepthDescription = Dx11DescBuilder::MakeDefaultDepthStencilDesc();
		return SUCCEEDED(device->CreateDepthStencilState(&defaultDepthDescription, &states.defaultDss));
	}

	bool Dx11Pipeline::Initialize(ID3D11Device* device, const SceneShaderRuntimeContract& shaderContract) {
		return device != nullptr && CreateShaders(device, shaderContract.builtIn, shaderContract.sceneInput)
			&& CreateStates(device);
	}

	void Dx11Pipeline::BindDefaultBlendState(ID3D11DeviceContext* context) const {
		if (context != nullptr)
			context->OMSetBlendState(states.blendState.Get(), nullptr, 0xffffffff);
	}

	ID3D11RasterizerState* Dx11Pipeline::ResolveModelRasterizerState(const bool bothFace) const {
		return bothFace ? states.bothFaceRs.Get() : states.frontFaceRs.Get();
	}

	void Dx11Pipeline::BindModel(ID3D11DeviceContext* context) const {
		if (context == nullptr)
			return;
		context->OMSetDepthStencilState(states.defaultDss.Get(), 0x00);
		context->OMSetBlendState(states.blendState.Get(), nullptr, 0xffffffff);
		context->IASetInputLayout(shaders.model.inputLayout.Get());
		context->VSSetShader(shaders.model.vertexShader.Get(), nullptr, 0);
		context->PSSetShader(shaders.model.pixelShader.Get(), nullptr, 0);
	}

	void Dx11Pipeline::BindEdge(ID3D11DeviceContext* context) const {
		if (context == nullptr)
			return;
		context->IASetInputLayout(shaders.edge.inputLayout.Get());
		context->VSSetShader(shaders.edge.vertexShader.Get(), nullptr, 0);
		context->PSSetShader(shaders.edge.pixelShader.Get(), nullptr, 0);
		context->RSSetState(states.edgeRs.Get());
		context->OMSetDepthStencilState(states.defaultDss.Get(), 0x00);
		context->OMSetBlendState(states.blendState.Get(), nullptr, 0xffffffff);
	}

	void Dx11Pipeline::BindGroundShadow(ID3D11DeviceContext* context) const {
		if (context == nullptr)
			return;
		context->IASetInputLayout(shaders.groundShadow.inputLayout.Get());
		context->VSSetShader(shaders.groundShadow.vertexShader.Get(), nullptr, 0);
		context->PSSetShader(shaders.groundShadow.pixelShader.Get(), nullptr, 0);
		context->RSSetState(states.gsRs.Get());
		context->OMSetDepthStencilState(states.gsDss.Get(), 0x01);
		context->OMSetBlendState(states.groundShadowBlendState.Get(), nullptr, 0xffffffff);
	}

	void Dx11Pipeline::BindSceneInput(ID3D11DeviceContext* context, const bool velocity) const {
		if (context == nullptr)
			return;
		const auto& [vertexShader, pixelShader, inputLayout] = velocity
			? static_cast<const Dx11Shader&>(shaders.sceneVelocity)
			: static_cast<const Dx11Shader&>(shaders.sceneDepth);
		context->IASetInputLayout(inputLayout.Get());
		context->VSSetShader(vertexShader.Get(), nullptr, 0);
		context->PSSetShader(pixelShader.Get(), nullptr, 0);
	}
}
