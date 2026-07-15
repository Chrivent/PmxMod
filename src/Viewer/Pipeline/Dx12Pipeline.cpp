#include "Viewer/Pipeline/Dx12Pipeline.h"

#include "Viewer/Pipeline/Dx12PipelineBuilder.h"
#include "Viewer/Geometry/ViewerGeometry.h"

#include <cstddef>
#include <iostream>
#include <limits>
#include <vector>

namespace Chrivent {
	void Dx12Pipeline::ConfigureAlphaBlend(D3D12_RENDER_TARGET_BLEND_DESC& blendDesc) {
		blendDesc.BlendEnable = TRUE;
		blendDesc.LogicOpEnable = FALSE;
		blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		blendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
		blendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	void Dx12Pipeline::ConfigureDefaultDepthStencil(D3D12_DEPTH_STENCIL_DESC& depthStencilDesc) {
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		depthStencilDesc.StencilEnable = FALSE;
	}

	bool Dx12Pipeline::CreateModelRootSignature(const Dx12Device& sourceDevice) {
		if (!sourceDevice.device)
			return false;
		D3D12_DESCRIPTOR_RANGE srvRange;
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 3;
		srvRange.BaseShaderRegister = 0;
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		D3D12_ROOT_PARAMETER rootParameters[3]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Descriptor.ShaderRegister = 0;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].Descriptor.ShaderRegister = 1;
		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[2].DescriptorTable.pDescriptorRanges = &srvRange;
		D3D12_STATIC_SAMPLER_DESC samplers[3]{};
		for (UINT index = 0; index < 3; index++) {
			auto& [Filter, AddressU, AddressV, AddressW,
				MipLODBias, MaxAnisotropy, ComparisonFunc, BorderColor,
				MinLOD, MaxLOD, ShaderRegister, RegisterSpace,
				ShaderVisibility] = samplers[index];
			Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			const D3D12_TEXTURE_ADDRESS_MODE addressMode = index == 1
				? D3D12_TEXTURE_ADDRESS_MODE_CLAMP
				: D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			AddressU = addressMode;
			AddressV = addressMode;
			AddressW = addressMode;
			MipLODBias = 0.0f;
			MaxAnisotropy = 1;
			ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			MinLOD = 0.0f;
			MaxLOD = D3D12_FLOAT32_MAX;
			ShaderRegister = index;
			RegisterSpace = 0;
			ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.NumParameters = 3;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.NumStaticSamplers = 3;
		rootSignatureDesc.pStaticSamplers = samplers;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return Dx12PipelineBuilder::CreateRootSignature(sourceDevice, rootSignatureDesc, modelRootSignature);
	}

	bool Dx12Pipeline::CreateModelPipelineStates(const Dx12Device& sourceDevice,
		const EffectPassDefinition& pass) {
		if (!sourceDevice.device || !modelRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		if (!Dx12PipelineBuilder::CompileShader(sourceDevice, pass.shaderPath, pass.vertexEntry,
			true, vertexShader, error)
			|| !Dx12PipelineBuilder::CompileShader(sourceDevice, pass.shaderPath, pass.pixelEntry,
				false, pixelShader, error)) {
			std::cerr << error;
			return false;
		}
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = modelRootSignature.Get();
		pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
		pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
		ConfigureAlphaBlend(pipelineDesc.BlendState.RenderTarget[0]);
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_BACK);
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.InputLayout = { inputElements, 3 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = sourceDevice.msaaSampleCount;
		if (FAILED(sourceDevice.device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&modelFrontFacePipelineState))))
			return false;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return SUCCEEDED(sourceDevice.device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&modelBothFacePipelineState)));
	}

	bool Dx12Pipeline::CreateDepthOnlyPipelineStates(
		const Dx12Device& sourceDevice, const EffectPassDefinition& pass) {
		if (!sourceDevice.device || !modelRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		if (!Dx12PipelineBuilder::CompileShader(sourceDevice, pass.shaderPath, pass.vertexEntry,
			true, vertexShader, error) || !Dx12PipelineBuilder::CompileShader(sourceDevice,
			pass.shaderPath, pass.pixelEntry, false, pixelShader, error)) {
			std::cerr << error;
			return false;
		}
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ViewerVertex, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = modelRootSignature.Get();
		pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
		pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_BACK);
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.InputLayout = { inputElements, 2 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 0;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = 1;
		if (FAILED(sourceDevice.device->CreateGraphicsPipelineState(
			&pipelineDesc, IID_PPV_ARGS(&depthOnlyFrontFacePipelineState))))
			return false;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return SUCCEEDED(sourceDevice.device->CreateGraphicsPipelineState(
			&pipelineDesc, IID_PPV_ARGS(&depthOnlyBothFacePipelineState)));
	}

	bool Dx12Pipeline::CreateSceneVelocityPipelineStates(const Dx12Device& sourceDevice,
		const EffectPassDefinition& pass) {
		if (!sourceDevice.device || !modelRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		if (!Dx12PipelineBuilder::CompileShader(sourceDevice, pass.shaderPath, pass.vertexEntry,
			true, vertexShader, error) || !Dx12PipelineBuilder::CompileShader(sourceDevice,
			pass.shaderPath, pass.pixelEntry, false, pixelShader, error)) {
			std::cerr << error;
			return false;
		}
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, previousPosition), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ViewerVertex, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = modelRootSignature.Get();
		pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
		pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
		pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN;
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_BACK);
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.InputLayout = { inputElements, 3 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R16G16_FLOAT;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = 1;
		if (FAILED(sourceDevice.device->CreateGraphicsPipelineState(
			&pipelineDesc, IID_PPV_ARGS(&sceneVelocityFrontFacePipelineState))))
			return false;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return SUCCEEDED(sourceDevice.device->CreateGraphicsPipelineState(
			&pipelineDesc, IID_PPV_ARGS(&sceneVelocityBothFacePipelineState)));
	}

	bool Dx12Pipeline::CreateEdgeRootSignature(const Dx12Device& sourceDevice) {
		if (!sourceDevice.device)
			return false;
		D3D12_ROOT_PARAMETER rootParameters[2]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Descriptor.ShaderRegister = 0;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].Descriptor.ShaderRegister = 1;
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return Dx12PipelineBuilder::CreateRootSignature(sourceDevice, rootSignatureDesc, edgeRootSignature);
	}

	bool Dx12Pipeline::CreateEdgePipelineState(const Dx12Device& sourceDevice,
		const EffectPassDefinition& pass) {
		if (!sourceDevice.device || !edgeRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		if (!Dx12PipelineBuilder::CompileShader(sourceDevice, pass.shaderPath, pass.vertexEntry,
			true, vertexShader, error)
			|| !Dx12PipelineBuilder::CompileShader(sourceDevice, pass.shaderPath, pass.pixelEntry,
				false, pixelShader, error)) {
			std::cerr << error;
			return false;
		}
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = edgeRootSignature.Get();
		pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
		pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
		ConfigureAlphaBlend(pipelineDesc.BlendState.RenderTarget[0]);
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_FRONT);
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.InputLayout = { inputElements, 2 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = sourceDevice.msaaSampleCount;
		return SUCCEEDED(sourceDevice.device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&edgePipelineState)));
	}

	bool Dx12Pipeline::CreateGroundShadowRootSignature(const Dx12Device& sourceDevice) {
		if (!sourceDevice.device)
			return false;
		D3D12_ROOT_PARAMETER rootParameters[2]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Descriptor.ShaderRegister = 0;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].Descriptor.ShaderRegister = 1;
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return Dx12PipelineBuilder::CreateRootSignature(sourceDevice, rootSignatureDesc, groundShadowRootSignature);
	}

	bool Dx12Pipeline::CreateGroundShadowPipelineState(const Dx12Device& sourceDevice,
		const EffectPassDefinition& pass) {
		if (!sourceDevice.device || !groundShadowRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		if (!Dx12PipelineBuilder::CompileShader(sourceDevice, pass.shaderPath, pass.vertexEntry,
			true, vertexShader, error)
			|| !Dx12PipelineBuilder::CompileShader(sourceDevice, pass.shaderPath, pass.pixelEntry,
				false, pixelShader, error)) {
			std::cerr << error;
			return false;
		}
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = groundShadowRootSignature.Get();
		pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
		pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
		ConfigureAlphaBlend(pipelineDesc.BlendState.RenderTarget[0]);
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_NONE);
		pipelineDesc.RasterizerState.DepthBias = -1;
		pipelineDesc.RasterizerState.DepthBiasClamp = -1.0f;
		pipelineDesc.RasterizerState.SlopeScaledDepthBias = -1.0f;
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.DepthStencilState.StencilEnable = TRUE;
		pipelineDesc.DepthStencilState.StencilReadMask = 0x01;
		pipelineDesc.DepthStencilState.StencilWriteMask = 0xFF;
		pipelineDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;
		pipelineDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		pipelineDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		pipelineDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		pipelineDesc.DepthStencilState.BackFace = pipelineDesc.DepthStencilState.FrontFace;
		pipelineDesc.InputLayout = { inputElements, 1 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = sourceDevice.msaaSampleCount;
		return SUCCEEDED(sourceDevice.device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&groundShadowPipelineState)));
	}

	bool Dx12Pipeline::Initialize(const Dx12Device& sourceDevice, const BuiltInShaderPasses& passes,
		const EffectPassDefinition& depthPass, const EffectPassDefinition& velocityPass) {
		Reset();
		if (!CreateModelRootSignature(sourceDevice))
			return false;
		if (!CreateModelPipelineStates(sourceDevice, passes.model))
			return false;
		if (!CreateDepthOnlyPipelineStates(sourceDevice, depthPass))
			return false;
		if (!CreateSceneVelocityPipelineStates(sourceDevice, velocityPass))
			return false;
		if (!CreateEdgeRootSignature(sourceDevice))
			return false;
		if (!CreateEdgePipelineState(sourceDevice, passes.edge))
			return false;
		if (!CreateGroundShadowRootSignature(sourceDevice))
			return false;
		return CreateGroundShadowPipelineState(sourceDevice, passes.groundShadow);
	}

	void Dx12Pipeline::BindModel(ID3D12GraphicsCommandList* commandList, const bool bothFace) const {
		if (commandList == nullptr)
			return;
		commandList->SetGraphicsRootSignature(modelRootSignature.Get());
		ID3D12PipelineState* pipelineState = bothFace
			? modelBothFacePipelineState.Get()
			: modelFrontFacePipelineState.Get();
		commandList->SetPipelineState(pipelineState);
	}

	void Dx12Pipeline::BindDepthOnly(ID3D12GraphicsCommandList* commandList, const bool bothFace) const {
		if (commandList == nullptr)
			return;
		commandList->SetGraphicsRootSignature(modelRootSignature.Get());
		ID3D12PipelineState* pipelineState = bothFace
			? depthOnlyBothFacePipelineState.Get()
			: depthOnlyFrontFacePipelineState.Get();
		commandList->SetPipelineState(pipelineState);
	}

	void Dx12Pipeline::BindSceneVelocity(ID3D12GraphicsCommandList* commandList, const bool bothFace) const {
		if (commandList == nullptr)
			return;
		commandList->SetGraphicsRootSignature(modelRootSignature.Get());
		commandList->SetPipelineState(bothFace
			? sceneVelocityBothFacePipelineState.Get() : sceneVelocityFrontFacePipelineState.Get());
	}

	void Dx12Pipeline::BindEdge(ID3D12GraphicsCommandList* commandList) const {
		if (commandList == nullptr)
			return;
		commandList->SetGraphicsRootSignature(edgeRootSignature.Get());
		commandList->SetPipelineState(edgePipelineState.Get());
	}

	void Dx12Pipeline::BindGroundShadow(ID3D12GraphicsCommandList* commandList) const {
		if (commandList == nullptr)
			return;
		commandList->SetGraphicsRootSignature(groundShadowRootSignature.Get());
		commandList->SetPipelineState(groundShadowPipelineState.Get());
	}

	void Dx12Pipeline::Reset() {
		groundShadowPipelineState.Reset();
		groundShadowRootSignature.Reset();
		edgePipelineState.Reset();
		edgeRootSignature.Reset();
		depthOnlyBothFacePipelineState.Reset();
		depthOnlyFrontFacePipelineState.Reset();
		sceneVelocityBothFacePipelineState.Reset();
		sceneVelocityFrontFacePipelineState.Reset();
		modelBothFacePipelineState.Reset();
		modelFrontFacePipelineState.Reset();
		modelRootSignature.Reset();
	}
}
