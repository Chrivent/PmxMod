#include "Viewer/Pipeline/Dx11Pipeline.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"

namespace Chrivent {
	bool Dx11Pipeline::CreateShaders(ID3D11Device* device, const BuiltInShaderPasses& builtInPasses,
		const SceneInputShaderPasses& sceneInputPasses) {
		return shaders.model.Initialize(device, builtInPasses.model)
			&& shaders.edge.Initialize(device, builtInPasses.edge)
			&& shaders.groundShadow.Initialize(device, builtInPasses.groundShadow)
			&& shaders.sceneDepth.Initialize(device, sceneInputPasses.depth)
			&& shaders.sceneVelocity.Initialize(device, sceneInputPasses.velocityInvertedY);
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

	bool Dx11Pipeline::Initialize(ID3D11Device* device, const BuiltInShaderPasses& builtInPasses,
		const SceneInputShaderPasses& sceneInputPasses) {
		return device != nullptr && CreateShaders(device, builtInPasses, sceneInputPasses)
			&& CreateStates(device);
	}
}
