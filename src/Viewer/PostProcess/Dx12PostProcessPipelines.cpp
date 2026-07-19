#include "Viewer/PostProcess/Dx12PostProcessPipelines.h"

#include "Viewer/Pipeline/Dx12PipelineBuilder.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"

#include <limits>

namespace Chrivent {
	GraphicsResult<void> Dx12PostProcessPipelines::CreateRootSignature(
		const Dx12Device& sourceDevice) {
		D3D12_DESCRIPTOR_RANGE srvRange{};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = PostProcessInputLayout::maxTextureCount;
		srvRange.BaseShaderRegister = PostProcessInputLayout::firstTextureRegister;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		D3D12_ROOT_PARAMETER rootParameters[3]{};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].Descriptor.ShaderRegister = PostProcessInputLayout::frameDataRegister;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[1].Descriptor.ShaderRegister = PostProcessInputLayout::parameterDataRegister;
		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[2].DescriptorTable.pDescriptorRanges = &srvRange;
		D3D12_STATIC_SAMPLER_DESC samplers[PostProcessInputLayout::samplerCount]{};
		for (UINT index = 0; index < PostProcessInputLayout::samplerCount; index++) {
			samplers[index].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplers[index].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplers[index].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplers[index].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplers[index].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			samplers[index].MaxLOD = D3D12_FLOAT32_MAX;
			samplers[index].ShaderRegister = index;
			samplers[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		D3D12_ROOT_SIGNATURE_DESC description;
		description.NumParameters = 3;
		description.pParameters = rootParameters;
		description.NumStaticSamplers = PostProcessInputLayout::samplerCount;
		description.pStaticSamplers = samplers;
		description.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return Dx12PipelineBuilder::CreateRootSignature(
			sourceDevice, description, rootSignature);
	}

	GraphicsResult<void> Dx12PostProcessPipelines::CreatePipelineState(
		const Dx12Device& sourceDevice,
		const ShaderProgramDefinition& program, const DXGI_FORMAT format,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState) const {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
		description.pRootSignature = rootSignature.Get();
		description.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		description.SampleMask = std::numeric_limits<UINT>::max();
		Dx12PipelineBuilder::ConfigureRasterizer(description.RasterizerState, D3D12_CULL_MODE_NONE);
		description.DepthStencilState.DepthEnable = FALSE;
		description.DepthStencilState.StencilEnable = FALSE;
		description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		description.NumRenderTargets = 1;
		description.RTVFormats[0] = format;
		description.SampleDesc.Count = 1;
		return Dx12PipelineBuilder::CreateGraphicsPipelineState(
			sourceDevice, program, description, pipelineState);
	}

	GraphicsResult<void> Dx12PostProcessPipelines::Initialize(
		const Dx12Device& sourceDevice,
		const std::span<const ShaderProgramDefinition> programs,
		const std::span<const DXGI_FORMAT> formats) {
		Reset();
		if (programs.empty())
			return {};
		if (!sourceDevice.GetDevice()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "후처리 pipeline 초기화",
				"DirectX 12 device를 사용할 수 없습니다"));
		}
		if (programs.size() != formats.size()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::DirectX12,
				GraphicsErrorCode::ContractViolation, "후처리 pipeline 초기화",
				"후처리 프로그램과 출력 형식 개수가 일치하지 않습니다"));
		}
		const auto rootSignatureResult = CreateRootSignature(sourceDevice);
		if (!rootSignatureResult)
			return std::unexpected(rootSignatureResult.error());
		for (size_t index = 0; index < programs.size(); index++) {
			Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
			const auto result = CreatePipelineState(
				sourceDevice, programs[index], formats[index], pipelineState);
			if (!result) {
				const GraphicsError error = result.error();
				Reset();
				return std::unexpected(error);
			}
			pipelineStates.emplace_back(std::move(pipelineState));
		}
		return {};
	}

	void Dx12PostProcessPipelines::Swap(Dx12PostProcessPipelines& other) noexcept {
		rootSignature.Swap(other.rootSignature);
		pipelineStates.swap(other.pipelineStates);
	}

	void Dx12PostProcessPipelines::Reset() {
		pipelineStates.clear();
		rootSignature.Reset();
	}
}
