#include "Viewer/Pipeline/Dx12Pipeline.h"

#include "Viewer/Pipeline/Dx12PipelineBuilder.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"

#include <cstddef>
#include <limits>

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

	GraphicsError::Result<void> Dx12Pipeline::CreateModelRootSignature(
		const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetDevice()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "모델 root signature 생성",
				"DirectX 12 device가 없어 모델 root signature를 만들 수 없습니다"));
		}
		D3D12_DESCRIPTOR_RANGE srvRange;
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = SceneShaderInputLayout::textureCount;
		srvRange.BaseShaderRegister = SceneShaderInputLayout::baseTextureRegister;
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		D3D12_ROOT_PARAMETER rootParameters[3]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Descriptor.ShaderRegister = SceneShaderInputLayout::vertexConstantRegister;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].Descriptor.ShaderRegister = SceneShaderInputLayout::pixelConstantRegister;
		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[2].DescriptorTable.pDescriptorRanges = &srvRange;
		D3D12_STATIC_SAMPLER_DESC samplers[SceneShaderInputLayout::samplerCount]{};
		for (UINT index = 0; index < SceneShaderInputLayout::samplerCount; index++) {
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
		rootSignatureDesc.NumStaticSamplers = SceneShaderInputLayout::samplerCount;
		rootSignatureDesc.pStaticSamplers = samplers;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return Dx12PipelineBuilder::CreateRootSignature(
			sourceDevice, rootSignatureDesc, modelRootSignature);
	}

	GraphicsError::Result<void> Dx12Pipeline::CreateModelPipelineStates(
		const Dx12Device& sourceDevice, const ShaderProgramDefinition& program) {
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = modelRootSignature.Get();
		ConfigureAlphaBlend(pipelineDesc.BlendState.RenderTarget[0]);
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_BACK);
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.InputLayout = { inputElements, 3 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = sourceDevice.GetMsaaSampleCount();
		auto result = Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, pipelineDesc, modelFrontFacePipelineState);
		if (!result)
			return result;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, pipelineDesc, modelBothFacePipelineState);
	}

	GraphicsError::Result<void> Dx12Pipeline::CreateSceneDepthPipelineStates(
		const Dx12Device& sourceDevice, const ShaderProgramDefinition& program) {
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ViewerVertex, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = modelRootSignature.Get();
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_BACK);
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.InputLayout = { inputElements, 2 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 0;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = 1;
		auto result = Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, pipelineDesc, sceneDepthFrontFacePipelineState);
		if (!result)
			return result;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, pipelineDesc, sceneDepthBothFacePipelineState);
	}

	GraphicsError::Result<void> Dx12Pipeline::CreateSceneVelocityPipelineStates(
		const Dx12Device& sourceDevice, const ShaderProgramDefinition& program) {
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(ViewerVertex, previousPosition), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ViewerVertex, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = modelRootSignature.Get();
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
		auto result = Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, pipelineDesc, sceneVelocityFrontFacePipelineState);
		if (!result)
			return result;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, pipelineDesc, sceneVelocityBothFacePipelineState);
	}

	GraphicsError::Result<void> Dx12Pipeline::CreateSimplePassRootSignature(
		const Dx12Device& sourceDevice) {
		if (!sourceDevice.GetDevice()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "단순 패스 root signature 생성",
				"DirectX 12 device가 없어 단순 패스 root signature를 만들 수 없습니다"));
		}
		D3D12_ROOT_PARAMETER rootParameters[2]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Descriptor.ShaderRegister = SceneShaderInputLayout::vertexConstantRegister;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].Descriptor.ShaderRegister = SceneShaderInputLayout::pixelConstantRegister;
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return Dx12PipelineBuilder::CreateRootSignature(
			sourceDevice, rootSignatureDesc, simplePassRootSignature);
	}

	GraphicsError::Result<void> Dx12Pipeline::CreateEdgePipelineState(
		const Dx12Device& sourceDevice, const ShaderProgramDefinition& program) {
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = simplePassRootSignature.Get();
		ConfigureAlphaBlend(pipelineDesc.BlendState.RenderTarget[0]);
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_FRONT);
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.InputLayout = { inputElements, 2 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = sourceDevice.GetMsaaSampleCount();
		return Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, pipelineDesc, edgePipelineState);
	}

	GraphicsError::Result<void> Dx12Pipeline::CreateGroundShadowPipelineState(
		const Dx12Device& sourceDevice, const ShaderProgramDefinition& program) {
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = simplePassRootSignature.Get();
		ConfigureAlphaBlend(pipelineDesc.BlendState.RenderTarget[0]);
		pipelineDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
		pipelineDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_NONE);
		pipelineDesc.RasterizerState.DepthBias = -1;
		pipelineDesc.RasterizerState.DepthBiasClamp = -1.0f;
		pipelineDesc.RasterizerState.SlopeScaledDepthBias = -1.0f;
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
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
		pipelineDesc.SampleDesc.Count = sourceDevice.GetMsaaSampleCount();
		return Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, pipelineDesc, groundShadowPipelineState);
	}

	void Dx12Pipeline::SwapResources(Dx12Pipeline& other) noexcept {
		modelRootSignature.Swap(other.modelRootSignature);
		modelFrontFacePipelineState.Swap(other.modelFrontFacePipelineState);
		modelBothFacePipelineState.Swap(other.modelBothFacePipelineState);
		sceneDepthFrontFacePipelineState.Swap(other.sceneDepthFrontFacePipelineState);
		sceneDepthBothFacePipelineState.Swap(other.sceneDepthBothFacePipelineState);
		sceneVelocityFrontFacePipelineState.Swap(other.sceneVelocityFrontFacePipelineState);
		sceneVelocityBothFacePipelineState.Swap(other.sceneVelocityBothFacePipelineState);
		simplePassRootSignature.Swap(other.simplePassRootSignature);
		edgePipelineState.Swap(other.edgePipelineState);
		groundShadowPipelineState.Swap(other.groundShadowPipelineState);
	}

	GraphicsError::Result<void> Dx12Pipeline::Initialize(const Dx12Device& sourceDevice,
		const SceneShaderRuntimeContract& shaderContract) {
		Dx12Pipeline candidate;
		auto result = candidate.CreateModelRootSignature(sourceDevice);
		if (result)
			result = candidate.CreateModelPipelineStates(
				sourceDevice, shaderContract.builtIn.model);
		if (result)
			result = candidate.CreateSceneDepthPipelineStates(
				sourceDevice, shaderContract.sceneInput.depth);
		if (result)
			result = candidate.CreateSceneVelocityPipelineStates(
				sourceDevice, shaderContract.sceneInput.velocity);
		if (result)
			result = candidate.CreateSimplePassRootSignature(sourceDevice);
		if (result)
			result = candidate.CreateEdgePipelineState(
				sourceDevice, shaderContract.builtIn.edge);
		if (result)
			result = candidate.CreateGroundShadowPipelineState(
				sourceDevice, shaderContract.builtIn.groundShadow);
		if (result)
			SwapResources(candidate);
		return result;
	}

	void Dx12Pipeline::BindModelRootSignature(ID3D12GraphicsCommandList* commandList) const {
		if (commandList == nullptr)
			return;
		commandList->SetGraphicsRootSignature(modelRootSignature.Get());
	}

	void Dx12Pipeline::BindModelPipelineState(ID3D12GraphicsCommandList* commandList, const bool bothFace) const {
		if (commandList == nullptr)
			return;
		ID3D12PipelineState* pipelineState = bothFace
			? modelBothFacePipelineState.Get()
			: modelFrontFacePipelineState.Get();
		commandList->SetPipelineState(pipelineState);
	}

	void Dx12Pipeline::BindSceneDepthPipelineState(ID3D12GraphicsCommandList* commandList,
		const bool bothFace) const {
		if (commandList == nullptr)
			return;
		ID3D12PipelineState* pipelineState = bothFace
			? sceneDepthBothFacePipelineState.Get()
			: sceneDepthFrontFacePipelineState.Get();
		commandList->SetPipelineState(pipelineState);
	}

	void Dx12Pipeline::BindSceneVelocityPipelineState(ID3D12GraphicsCommandList* commandList, const bool bothFace) const {
		if (commandList == nullptr)
			return;
		commandList->SetPipelineState(bothFace
			? sceneVelocityBothFacePipelineState.Get() : sceneVelocityFrontFacePipelineState.Get());
	}

	void Dx12Pipeline::BindEdge(ID3D12GraphicsCommandList* commandList) const {
		if (commandList == nullptr)
			return;
		commandList->SetGraphicsRootSignature(simplePassRootSignature.Get());
		commandList->SetPipelineState(edgePipelineState.Get());
	}

	void Dx12Pipeline::BindGroundShadow(ID3D12GraphicsCommandList* commandList) const {
		if (commandList == nullptr)
			return;
		commandList->SetGraphicsRootSignature(simplePassRootSignature.Get());
		commandList->SetPipelineState(groundShadowPipelineState.Get());
	}

	void Dx12Pipeline::Reset() {
		groundShadowPipelineState.Reset();
		edgePipelineState.Reset();
		simplePassRootSignature.Reset();
		sceneDepthBothFacePipelineState.Reset();
		sceneDepthFrontFacePipelineState.Reset();
		sceneVelocityBothFacePipelineState.Reset();
		sceneVelocityFrontFacePipelineState.Reset();
		modelBothFacePipelineState.Reset();
		modelFrontFacePipelineState.Reset();
		modelRootSignature.Reset();
	}
}
