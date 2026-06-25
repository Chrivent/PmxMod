#include "Viewer/Dx12/Helper/Dx12Pipeline.h"

#include "Viewer/Shader/DxcShaderCompiler.h"
#include "Viewer/Shader/PostProcessInputLayout.h"
#include "Viewer/Shader/ShaderCompiler.h"

#include <iostream>
#include <limits>
#include <utility>

namespace Chrivent {
	bool Dx12Pipeline::CreateRootSignature(
		const Dx12Device& sourceDevice,
		const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature) {
		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob))) {
			if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr)
				std::cerr << static_cast<const char*>(errorBlob->GetBufferPointer()) << '\n';
			return false;
		}
		return SUCCEEDED(sourceDevice.device->CreateRootSignature(0,
			signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature)));
	}

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

	void Dx12Pipeline::ConfigureRasterizer(D3D12_RASTERIZER_DESC& rasterizerDesc, const D3D12_CULL_MODE cullMode) {
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = cullMode;
		rasterizerDesc.FrontCounterClockwise = TRUE;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.DepthClipEnable = TRUE;
	}

	void Dx12Pipeline::ConfigureDefaultDepthStencil(D3D12_DEPTH_STENCIL_DESC& depthStencilDesc) {
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		depthStencilDesc.StencilEnable = FALSE;
	}

	bool Dx12Pipeline::CompileShader(const Dx12Device& sourceDevice, const std::filesystem::path& file,
		const std::string& entry, const bool vertexShader, std::vector<uint8_t>& bytecode, std::string& error) {
		if (sourceDevice.capabilities.shaderModelMajor >= 6) {
			const std::wstring wideEntry(entry.begin(), entry.end());
			return DxcShaderCompiler::CompileDxil(
				file, wideEntry, vertexShader ? L"vs_6_0" : L"ps_6_0", bytecode, error);
		}
		Microsoft::WRL::ComPtr<ID3DBlob> legacyBytecode;
		if (!ShaderCompiler::CompileFile(file, entry.c_str(), vertexShader ? "vs_5_1" : "ps_5_1",
			legacyBytecode, error))
			return false;
		bytecode.resize(legacyBytecode->GetBufferSize());
		std::memcpy(bytecode.data(), legacyBytecode->GetBufferPointer(), bytecode.size());
		return true;
	}

	bool Dx12Pipeline::CreateModelRootSignature(const Dx12Device& sourceDevice) {
		if (!sourceDevice.device)
			return false;
		D3D12_DESCRIPTOR_RANGE srvRange;
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 3;
		srvRange.BaseShaderRegister = PostProcessInputLayout::SceneColorRegister;
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
		return CreateRootSignature(sourceDevice, rootSignatureDesc, modelRootSignature);
	}

	bool Dx12Pipeline::CreateModelPipelineStates(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir) {
		if (!sourceDevice.device || !modelRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		const auto shaderPath = shaderDir / "model/effect.hlsl";
		if (!CompileShader(sourceDevice, shaderPath, "VSMain", true, vertexShader, error) ||
			!CompileShader(sourceDevice, shaderPath, "PSMain", false, pixelShader, error)) {
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
		ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_BACK);
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
		const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir) {
		if (!sourceDevice.device || !modelRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::string error;
		const auto shaderPath = shaderDir / "model/effect.hlsl";
		if (!CompileShader(sourceDevice, shaderPath, "VSMain", true, vertexShader, error)) {
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
		pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
		ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_BACK);
		ConfigureDefaultDepthStencil(pipelineDesc.DepthStencilState);
		pipelineDesc.InputLayout = { inputElements, 3 };
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
		return CreateRootSignature(sourceDevice, rootSignatureDesc, edgeRootSignature);
	}

	bool Dx12Pipeline::CreateEdgePipelineState(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir) {
		if (!sourceDevice.device || !edgeRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		const auto shaderPath = shaderDir / "edge/effect.hlsl";
		if (!CompileShader(sourceDevice, shaderPath, "VSMain", true, vertexShader, error) ||
			!CompileShader(sourceDevice, shaderPath, "PSMain", false, pixelShader, error)) {
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
		ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_FRONT);
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
		return CreateRootSignature(sourceDevice, rootSignatureDesc, groundShadowRootSignature);
	}

	bool Dx12Pipeline::CreateGroundShadowPipelineState(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir) {
		if (!sourceDevice.device || !groundShadowRootSignature)
			return false;
		std::vector<uint8_t> vertexShader;
		std::vector<uint8_t> pixelShader;
		std::string error;
		const auto shaderPath = shaderDir / "ground-shadow/effect.hlsl";
		if (!CompileShader(sourceDevice, shaderPath, "VSMain", true, vertexShader, error) ||
			!CompileShader(sourceDevice, shaderPath, "PSMain", false, pixelShader, error)) {
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
		ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_NONE);
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

	bool Dx12Pipeline::CreatePostProcessRootSignature(const Dx12Device& sourceDevice) {
		D3D12_DESCRIPTOR_RANGE srvRange{};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = PostProcessInputLayout::RequiredTextureCount;
		srvRange.BaseShaderRegister = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		D3D12_ROOT_PARAMETER rootParameter;
		rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameter.DescriptorTable.NumDescriptorRanges = 1;
		rootParameter.DescriptorTable.pDescriptorRanges = &srvRange;
		D3D12_STATIC_SAMPLER_DESC sampler{};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;
		sampler.ShaderRegister = PostProcessInputLayout::LinearClampSamplerRegister;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.NumParameters = 1;
		rootSignatureDesc.pParameters = &rootParameter;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &sampler;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return CreateRootSignature(sourceDevice, rootSignatureDesc, postProcessRootSignature);
	}

	bool Dx12Pipeline::Initialize(const Dx12Device& sourceDevice, const std::filesystem::path& shaderDir) {
		Reset();
		if (!CreateModelRootSignature(sourceDevice))
			return false;
		if (!CreateModelPipelineStates(sourceDevice, shaderDir))
			return false;
		if (!CreateDepthOnlyPipelineStates(sourceDevice, shaderDir))
			return false;
		if (!CreateEdgeRootSignature(sourceDevice))
			return false;
		if (!CreateEdgePipelineState(sourceDevice, shaderDir))
			return false;
		if (!CreateGroundShadowRootSignature(sourceDevice))
			return false;
		return CreateGroundShadowPipelineState(sourceDevice, shaderDir);
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

	bool Dx12Pipeline::LoadPostProcessEffects(const Dx12Device& sourceDevice, const std::vector<const EffectDefinition*>& effects) {
		if (!sourceDevice.device)
			return false;
		postProcessPipelineStates.clear();
		postProcessRootSignature.Reset();
		if (effects.empty())
			return true;
		if (!CreatePostProcessRootSignature(sourceDevice))
			return false;
		for (const auto* effect : effects) {
			if (!effect || effect->passes.empty())
				continue;
			const auto& pass = effect->passes.front();
			std::vector<uint8_t> vertexShader;
			std::vector<uint8_t> pixelShader;
			std::string error;
			if (!CompileShader(sourceDevice, pass.shaderPath, pass.vertexEntry, true, vertexShader, error)
				|| !CompileShader(sourceDevice, pass.shaderPath, pass.pixelEntry, false, pixelShader, error)) {
				std::cerr << error;
				ClearPostProcessEffects();
				return false;
			}
			D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
			pipelineDesc.pRootSignature = postProcessRootSignature.Get();
			pipelineDesc.VS = { vertexShader.data(), vertexShader.size() };
			pipelineDesc.PS = { pixelShader.data(), pixelShader.size() };
			pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			pipelineDesc.SampleMask = std::numeric_limits<UINT>::max();
			ConfigureRasterizer(pipelineDesc.RasterizerState, D3D12_CULL_MODE_NONE);
			pipelineDesc.DepthStencilState.DepthEnable = FALSE;
			pipelineDesc.DepthStencilState.StencilEnable = FALSE;
			pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			pipelineDesc.NumRenderTargets = 1;
			pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			pipelineDesc.SampleDesc.Count = 1;
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
			if (FAILED(sourceDevice.device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState)))) {
				ClearPostProcessEffects();
				return false;
			}
			postProcessPipelineStates.push_back(std::move(pipelineState));
		}
		return true;
	}

	void Dx12Pipeline::BindPostProcess(ID3D12GraphicsCommandList* commandList,
		const size_t passIndex, const D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle) const {
		if (commandList == nullptr || !postProcessRootSignature || passIndex >= postProcessPipelineStates.size())
			return;
		commandList->SetGraphicsRootSignature(postProcessRootSignature.Get());
		commandList->SetPipelineState(postProcessPipelineStates[passIndex].Get());
		commandList->SetGraphicsRootDescriptorTable(0, sceneColorHandle);
	}

	void Dx12Pipeline::ClearPostProcessEffects() {
		postProcessPipelineStates.clear();
		postProcessRootSignature.Reset();
	}

	void Dx12Pipeline::Reset() {
		postProcessPipelineStates.clear();
		postProcessRootSignature.Reset();
		groundShadowPipelineState.Reset();
		groundShadowRootSignature.Reset();
		edgePipelineState.Reset();
		edgeRootSignature.Reset();
		depthOnlyBothFacePipelineState.Reset();
		depthOnlyFrontFacePipelineState.Reset();
		modelBothFacePipelineState.Reset();
		modelFrontFacePipelineState.Reset();
		modelRootSignature.Reset();
	}
}
