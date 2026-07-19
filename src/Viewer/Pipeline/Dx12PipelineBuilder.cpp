#include "Viewer/Pipeline/Dx12PipelineBuilder.h"

#include "Viewer/Shader/DxcHlslCompiler.h"
#include "Viewer/Shader/D3DCompilerHlslCompiler.h"

#include <utility>

namespace Chrivent {
	GraphicsError::Result<void> Dx12PipelineBuilder::CreateRootSignature(
		const Dx12Device& sourceDevice,
		const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature) {
		if (!sourceDevice.GetDevice()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "root signature 생성",
				"DirectX 12 device를 사용할 수 없습니다"));
		}
		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		HRESULT result = D3D12SerializeRootSignature(&rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
		if (FAILED(result)) {
			std::string message = errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr
				? static_cast<const char*>(errorBlob->GetBufferPointer())
				: "DirectX 12 root signature를 직렬화하지 못했습니다";
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::ContractViolation, "root signature 직렬화",
				std::move(message), result, true));
		}
		result = sourceDevice.GetDevice()->CreateRootSignature(0,
			signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature));
		if (SUCCEEDED(result))
			return {};
		return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
			GraphicsErrorCode::ResourceCreationFailed, "root signature 생성",
			"DirectX 12 root signature 리소스를 만들지 못했습니다", result, true));
	}

	void Dx12PipelineBuilder::ConfigureRasterizer(D3D12_RASTERIZER_DESC& rasterizerDesc,
		const D3D12_CULL_MODE cullMode) {
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = cullMode;
		rasterizerDesc.FrontCounterClockwise = TRUE;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.DepthClipEnable = TRUE;
	}

	GraphicsError::Result<std::vector<uint8_t>> Dx12PipelineBuilder::CompileShader(
		const Dx12Device& sourceDevice, const std::filesystem::path& file,
		const std::string& entry, const bool vertexShader) {
		if (sourceDevice.GetMaximumShaderModel() >= D3D_SHADER_MODEL_6_0) {
			const std::wstring wideEntry(entry.begin(), entry.end());
			auto result = DxcHlslCompiler::CompileDxil(
				file, wideEntry, vertexShader ? L"vs_6_0" : L"ps_6_0");
			if (result)
				return std::move(*result);
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::EffectConfigurationFailed, "DXIL 셰이더 컴파일",
				std::move(result.error().message)));
		}
		auto result = D3DCompilerHlslCompiler::CompileFile(
			file, entry.c_str(), vertexShader ? "vs_5_1" : "ps_5_1");
		if (!result) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::EffectConfigurationFailed, "레거시 셰이더 컴파일",
				std::move(result.error().message)));
		}
		const Microsoft::WRL::ComPtr<ID3DBlob>& legacyBytecode = *result;
		std::vector<uint8_t> bytecode;
		bytecode.resize(legacyBytecode->GetBufferSize());
		std::memcpy(bytecode.data(), legacyBytecode->GetBufferPointer(), bytecode.size());
		return bytecode;
	}

	GraphicsError::Result<void> Dx12PipelineBuilder::CreateGraphicsPipelineState(
		const Dx12Device& sourceDevice,
		const ShaderProgramDefinition& program,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC& description,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState) {
		pipelineState.Reset();
		if (!sourceDevice.GetDevice() || description.pRootSignature == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
				GraphicsErrorCode::InvalidArgument, "graphics pipeline state 생성",
				"DirectX 12 graphics pipeline 생성 설정이 올바르지 않습니다"));
		}
		auto vertexShaderResult = CompileShader(
			sourceDevice, program.shaderPath, program.vertexEntry, true);
		if (!vertexShaderResult)
			return std::unexpected(vertexShaderResult.error());
		auto pixelShaderResult = CompileShader(
			sourceDevice, program.shaderPath, program.pixelEntry, false);
		if (!pixelShaderResult)
			return std::unexpected(pixelShaderResult.error());
		const std::vector<uint8_t>& vertexShader = *vertexShaderResult;
		const std::vector<uint8_t>& pixelShader = *pixelShaderResult;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC completedDescription = description;
		completedDescription.VS = { vertexShader.data(), vertexShader.size() };
		completedDescription.PS = { pixelShader.data(), pixelShader.size() };
		const HRESULT result = sourceDevice.GetDevice()->CreateGraphicsPipelineState(
			&completedDescription, IID_PPV_ARGS(&pipelineState));
		if (SUCCEEDED(result))
			return {};
		return std::unexpected(GraphicsError::Create(GraphicsApi::DirectX12,
			GraphicsErrorCode::ResourceCreationFailed, "graphics pipeline state 생성",
			"DirectX 12 graphics pipeline state를 만들지 못했습니다", result, true));
	}
}
