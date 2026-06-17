#include "Dx12Pipeline.h"

#include "Dx12ShaderCompiler.h"

#include <iostream>
#include <limits>

namespace Chrivent {
	bool Dx12Pipeline::CreateModelRootSignature(const Dx12DeviceInfo& deviceInfo) {
		if (!deviceInfo.device)
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
		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		if (FAILED(D3D12SerializeRootSignature(
			&rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob,
			&errorBlob))) {
			if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr)
				std::cerr << static_cast<const char*>(errorBlob->GetBufferPointer()) << '\n';
			return false;
		}
		return SUCCEEDED(deviceInfo.device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&modelRootSignature)));
	}

	bool Dx12Pipeline::CreateModelPipelineStates(const Dx12DeviceInfo& deviceInfo, const std::filesystem::path& shaderDir) {
		if (!deviceInfo.device || !modelRootSignature)
			return false;
		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
		std::string error;
		const auto shaderPath = shaderDir / "model.hlsl";
		if (!Dx12ShaderCompiler::CompileFile(shaderPath, "VSMain", "vs_5_1", vertexShader, error) ||
			!Dx12ShaderCompiler::CompileFile(shaderPath, "PSMain", "ps_5_1", pixelShader, error)) {
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
		pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		auto& [BlendEnable, LogicOpEnable, SrcBlend, DestBlend,
			BlendOp, SrcBlendAlpha, DestBlendAlpha, BlendOpAlpha, LogicOp,
			RenderTargetWriteMask] = pipelineDesc.BlendState.RenderTarget[0];
		BlendEnable = TRUE;
		LogicOpEnable = FALSE;
		SrcBlend = D3D12_BLEND_SRC_ALPHA;
		DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		BlendOp = D3D12_BLEND_OP_ADD;
		SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		BlendOpAlpha = D3D12_BLEND_OP_ADD;
		LogicOp = D3D12_LOGIC_OP_NOOP;
		RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipelineDesc.SampleMask = (std::numeric_limits<UINT>::max)();
		pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		pipelineDesc.RasterizerState.FrontCounterClockwise = TRUE;
		pipelineDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		pipelineDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		pipelineDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		pipelineDesc.RasterizerState.DepthClipEnable = TRUE;
		pipelineDesc.DepthStencilState.DepthEnable = TRUE;
		pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		pipelineDesc.DepthStencilState.StencilEnable = FALSE;
		pipelineDesc.InputLayout = { inputElements, 3 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = deviceInfo.msaaSampleCount;
		if (FAILED(deviceInfo.device->CreateGraphicsPipelineState(
			&pipelineDesc,
			IID_PPV_ARGS(&modelFrontFacePipelineState))))
			return false;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		return SUCCEEDED(deviceInfo.device->CreateGraphicsPipelineState(
			&pipelineDesc,
			IID_PPV_ARGS(&modelBothFacePipelineState)));
	}

	bool Dx12Pipeline::CreateEdgeRootSignature(const Dx12DeviceInfo& deviceInfo) {
		if (!deviceInfo.device)
			return false;
		D3D12_ROOT_PARAMETER rootParameters[3]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[0].Descriptor.ShaderRegister = 0;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[1].Descriptor.ShaderRegister = 1;
		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[2].Descriptor.ShaderRegister = 2;
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.NumParameters = 3;
		rootSignatureDesc.pParameters = rootParameters;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		if (FAILED(D3D12SerializeRootSignature(
			&rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob,
			&errorBlob))) {
			if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr)
				std::cerr << static_cast<const char*>(errorBlob->GetBufferPointer()) << '\n';
			return false;
		}
		return SUCCEEDED(deviceInfo.device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&edgeRootSignature)));
	}

	bool Dx12Pipeline::CreateEdgePipelineState(const Dx12DeviceInfo& deviceInfo, const std::filesystem::path& shaderDir) {
		if (!deviceInfo.device || !edgeRootSignature)
			return false;
		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
		std::string error;
		const auto shaderPath = shaderDir / "edge.hlsl";
		if (!Dx12ShaderCompiler::CompileFile(shaderPath, "VSMain", "vs_5_1", vertexShader, error) ||
			!Dx12ShaderCompiler::CompileFile(shaderPath, "PSMain", "ps_5_1", pixelShader, error)) {
			std::cerr << error;
			return false;
		}
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = edgeRootSignature.Get();
		pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		pipelineDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		pipelineDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		pipelineDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipelineDesc.SampleMask = (std::numeric_limits<UINT>::max)();
		pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		pipelineDesc.RasterizerState.FrontCounterClockwise = TRUE;
		pipelineDesc.RasterizerState.DepthClipEnable = TRUE;
		pipelineDesc.DepthStencilState.DepthEnable = TRUE;
		pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		pipelineDesc.DepthStencilState.StencilEnable = FALSE;
		pipelineDesc.InputLayout = { inputElements, 2 };
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		pipelineDesc.SampleDesc.Count = deviceInfo.msaaSampleCount;
		return SUCCEEDED(deviceInfo.device->CreateGraphicsPipelineState(
			&pipelineDesc,
			IID_PPV_ARGS(&edgePipelineState)));
	}

	bool Dx12Pipeline::CreateGroundShadowRootSignature(const Dx12DeviceInfo& deviceInfo) {
		if (!deviceInfo.device)
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
		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		if (FAILED(D3D12SerializeRootSignature(
			&rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob,
			&errorBlob))) {
			if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr)
				std::cerr << static_cast<const char*>(errorBlob->GetBufferPointer()) << '\n';
			return false;
		}
		return SUCCEEDED(deviceInfo.device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&groundShadowRootSignature)));
	}

	bool Dx12Pipeline::CreateGroundShadowPipelineState(const Dx12DeviceInfo& deviceInfo, const std::filesystem::path& shaderDir) {
		if (!deviceInfo.device || !groundShadowRootSignature)
			return false;
		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
		std::string error;
		const auto shaderPath = shaderDir / "ground_shadow.hlsl";
		if (!Dx12ShaderCompiler::CompileFile(shaderPath, "VSMain", "vs_5_1", vertexShader, error) ||
			!Dx12ShaderCompiler::CompileFile(shaderPath, "PSMain", "ps_5_1", pixelShader, error)) {
			std::cerr << error;
			return false;
		}
		D3D12_INPUT_ELEMENT_DESC inputElements[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
		pipelineDesc.pRootSignature = groundShadowRootSignature.Get();
		pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		pipelineDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		pipelineDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		pipelineDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		pipelineDesc.SampleMask = (std::numeric_limits<UINT>::max)();
		pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pipelineDesc.RasterizerState.FrontCounterClockwise = TRUE;
		pipelineDesc.RasterizerState.DepthBias = -1;
		pipelineDesc.RasterizerState.DepthBiasClamp = -1.0f;
		pipelineDesc.RasterizerState.SlopeScaledDepthBias = -1.0f;
		pipelineDesc.RasterizerState.DepthClipEnable = TRUE;
		pipelineDesc.DepthStencilState.DepthEnable = TRUE;
		pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
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
		pipelineDesc.SampleDesc.Count = deviceInfo.msaaSampleCount;
		return SUCCEEDED(deviceInfo.device->CreateGraphicsPipelineState(
			&pipelineDesc,
			IID_PPV_ARGS(&groundShadowPipelineState)));
	}

	bool Dx12Pipeline::Initialize(const Dx12DeviceInfo& deviceInfo, const std::filesystem::path& shaderDir) {
		Destroy();
		if (!CreateModelRootSignature(deviceInfo))
			return false;
		if (!CreateModelPipelineStates(deviceInfo, shaderDir))
			return false;
		if (!CreateEdgeRootSignature(deviceInfo))
			return false;
		if (!CreateEdgePipelineState(deviceInfo, shaderDir))
			return false;
		if (!CreateGroundShadowRootSignature(deviceInfo))
			return false;
		return CreateGroundShadowPipelineState(deviceInfo, shaderDir);
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

	void Dx12Pipeline::Destroy() {
		groundShadowPipelineState.Reset();
		groundShadowRootSignature.Reset();
		edgePipelineState.Reset();
		edgeRootSignature.Reset();
		modelBothFacePipelineState.Reset();
		modelFrontFacePipelineState.Reset();
		modelRootSignature.Reset();
	}
}
