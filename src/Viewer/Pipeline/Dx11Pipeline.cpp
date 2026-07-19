#include "Viewer/Pipeline/Dx11Pipeline.h"

#include "Viewer/Descriptor/Dx11DescBuilder.h"
#include "Viewer/Geometry/ViewerGeometry.h"

#include <cstddef>
#include <utility>

namespace Chrivent {
	GraphicsResult<void> Dx11Pipeline::CreateShaders(ID3D11Device* device,
		const BuiltInShaderPasses& builtInPasses,
		const SceneInputShaderPasses& sceneInputPasses) {
		constexpr D3D11_INPUT_ELEMENT_DESC modelInputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				offsetof(ViewerVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				offsetof(ViewerVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		constexpr D3D11_INPUT_ELEMENT_DESC edgeInputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				offsetof(ViewerVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		constexpr D3D11_INPUT_ELEMENT_DESC groundShadowInputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		constexpr D3D11_INPUT_ELEMENT_DESC sceneDepthInputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				offsetof(ViewerVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		constexpr D3D11_INPUT_ELEMENT_DESC sceneVelocityInputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				offsetof(ViewerVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0,
				offsetof(ViewerVertex, previousPosition), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
				offsetof(ViewerVertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		auto result = shaders.model.Initialize(
			device, builtInPasses.model, modelInputElements);
		if (result)
			result = shaders.edge.Initialize(
				device, builtInPasses.edge, edgeInputElements);
		if (result)
			result = shaders.groundShadow.Initialize(
				device, builtInPasses.groundShadow, groundShadowInputElements);
		if (result)
			result = shaders.sceneDepth.Initialize(
				device, sceneInputPasses.depth, sceneDepthInputElements);
		if (result)
			result = shaders.sceneVelocity.Initialize(
				device, sceneInputPasses.velocity, sceneVelocityInputElements);
		return result;
	}

	GraphicsResult<void> Dx11Pipeline::CreateStates(ID3D11Device* device) {
		auto wrapLinear = Dx11DescBuilder::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR,
			D3D11_TEXTURE_ADDRESS_WRAP);
		HRESULT result = device->CreateSamplerState(&wrapLinear, &states.textureSampler);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "일반 texture sampler 생성",
				"DirectX 11 일반 texture sampler를 만들지 못했습니다", result, true));
		}
		auto clampLinear = Dx11DescBuilder::MakeSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR,
			D3D11_TEXTURE_ADDRESS_CLAMP);
		result = device->CreateSamplerState(&clampLinear, &states.toonTextureSampler);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "툰 texture sampler 생성",
				"DirectX 11 툰 texture sampler를 만들지 못했습니다", result, true));
		}
		auto blend = Dx11DescBuilder::MakeAlphaBlendDesc();
		result = device->CreateBlendState(&blend, &states.blendState);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "alpha blend state 생성",
				"DirectX 11 alpha blend state를 만들지 못했습니다", result, true));
		}
		auto groundShadowBlend = Dx11DescBuilder::MakeGroundShadowBlendDesc();
		result = device->CreateBlendState(&groundShadowBlend, &states.groundShadowBlendState);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "지면 그림자 blend state 생성",
				"DirectX 11 지면 그림자 blend state를 만들지 못했습니다", result, true));
		}
		auto frontDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_BACK, true);
		result = device->CreateRasterizerState(&frontDescription, &states.frontFaceRs);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "단면 rasterizer state 생성",
				"DirectX 11 단면 rasterizer state를 만들지 못했습니다", result, true));
		}
		auto bothDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		result = device->CreateRasterizerState(&bothDescription, &states.bothFaceRs);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "양면 rasterizer state 생성",
				"DirectX 11 양면 rasterizer state를 만들지 못했습니다", result, true));
		}
		auto edgeDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_FRONT, true);
		result = device->CreateRasterizerState(&edgeDescription, &states.edgeRs);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "엣지 rasterizer state 생성",
				"DirectX 11 엣지 rasterizer state를 만들지 못했습니다", result, true));
		}
		auto shadowDescription = Dx11DescBuilder::MakeRasterizerDesc(D3D11_CULL_NONE, true);
		shadowDescription.DepthBias = -1;
		shadowDescription.SlopeScaledDepthBias = -1.0f;
		shadowDescription.DepthBiasClamp = -1.0f;
		result = device->CreateRasterizerState(&shadowDescription, &states.gsRs);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "지면 그림자 rasterizer state 생성",
				"DirectX 11 지면 그림자 rasterizer state를 만들지 못했습니다", result, true));
		}
		auto shadowDepthDescription = Dx11DescBuilder::MakeGroundShadowDepthStencilDesc();
		result = device->CreateDepthStencilState(&shadowDepthDescription, &states.gsDss);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "지면 그림자 depth state 생성",
				"DirectX 11 지면 그림자 depth state를 만들지 못했습니다", result, true));
		}
		auto defaultDepthDescription = Dx11DescBuilder::MakeDefaultDepthStencilDesc();
		result = device->CreateDepthStencilState(&defaultDepthDescription, &states.defaultDss);
		if (FAILED(result)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::ResourceCreationFailed, "기본 depth state 생성",
				"DirectX 11 기본 depth state를 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> Dx11Pipeline::Initialize(ID3D11Device* device,
		const SceneShaderRuntimeContract& shaderContract) {
		if (device == nullptr)
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX11,
				GraphicsErrorCode::InvalidArgument, "rendering pipeline 초기화",
				"DirectX 11 device가 올바르지 않습니다"));
		Dx11Pipeline candidate;
		const auto shaderResult = candidate.CreateShaders(
			device, shaderContract.builtIn, shaderContract.sceneInput);
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		const auto stateResult = candidate.CreateStates(device);
		if (!stateResult)
			return std::unexpected(stateResult.error());
		shaders = std::move(candidate.shaders);
		states = std::move(candidate.states);
		return {};
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
		context->IASetInputLayout(shaders.model.GetInputLayout());
		context->VSSetShader(shaders.model.GetVertexShader(), nullptr, 0);
		context->PSSetShader(shaders.model.GetPixelShader(), nullptr, 0);
	}

	void Dx11Pipeline::BindEdge(ID3D11DeviceContext* context) const {
		if (context == nullptr)
			return;
		context->IASetInputLayout(shaders.edge.GetInputLayout());
		context->VSSetShader(shaders.edge.GetVertexShader(), nullptr, 0);
		context->PSSetShader(shaders.edge.GetPixelShader(), nullptr, 0);
		context->RSSetState(states.edgeRs.Get());
		context->OMSetDepthStencilState(states.defaultDss.Get(), 0x00);
		context->OMSetBlendState(states.blendState.Get(), nullptr, 0xffffffff);
	}

	void Dx11Pipeline::BindGroundShadow(ID3D11DeviceContext* context) const {
		if (context == nullptr)
			return;
		context->IASetInputLayout(shaders.groundShadow.GetInputLayout());
		context->VSSetShader(shaders.groundShadow.GetVertexShader(), nullptr, 0);
		context->PSSetShader(shaders.groundShadow.GetPixelShader(), nullptr, 0);
		context->RSSetState(states.gsRs.Get());
		context->OMSetDepthStencilState(states.gsDss.Get(), 0x01);
		context->OMSetBlendState(states.groundShadowBlendState.Get(), nullptr, 0xffffffff);
	}

	void Dx11Pipeline::BindSceneDepth(ID3D11DeviceContext* context) const {
		if (context == nullptr)
			return;
		context->IASetInputLayout(shaders.sceneDepth.GetInputLayout());
		context->VSSetShader(shaders.sceneDepth.GetVertexShader(), nullptr, 0);
		context->PSSetShader(shaders.sceneDepth.GetPixelShader(), nullptr, 0);
	}

	void Dx11Pipeline::BindSceneVelocity(ID3D11DeviceContext* context) const {
		if (context == nullptr)
			return;
		context->IASetInputLayout(shaders.sceneVelocity.GetInputLayout());
		context->VSSetShader(shaders.sceneVelocity.GetVertexShader(), nullptr, 0);
		context->PSSetShader(shaders.sceneVelocity.GetPixelShader(), nullptr, 0);
	}
}
