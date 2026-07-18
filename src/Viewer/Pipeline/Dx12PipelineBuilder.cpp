#include "Viewer/Pipeline/Dx12PipelineBuilder.h"

#include "Viewer/Shader/ModernHlslCompiler.h"
#include "Viewer/Shader/LegacyHlslCompiler.h"

namespace Chrivent {
	bool Dx12PipelineBuilder::CreateRootSignature(const Dx12Device& sourceDevice,
		const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc,
		Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature, std::string& error) {
		error.clear();
		Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob))) {
			error = errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr
				? static_cast<const char*>(errorBlob->GetBufferPointer())
				: "DirectX 12 root signature를 직렬화하지 못했습니다";
			return false;
		}
		if (SUCCEEDED(sourceDevice.GetDevice()->CreateRootSignature(0,
			signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&rootSignature))))
			return true;
		error = "DirectX 12 root signature 리소스를 만들지 못했습니다";
		return false;
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

	bool Dx12PipelineBuilder::CompileShader(const Dx12Device& sourceDevice, const std::filesystem::path& file,
		const std::string& entry, const bool vertexShader, std::vector<uint8_t>& bytecode, std::string& error) {
		if (sourceDevice.GetMaximumShaderModel() >= D3D_SHADER_MODEL_6_0) {
			const std::wstring wideEntry(entry.begin(), entry.end());
			return ModernHlslCompiler::CompileDxil(
				file, wideEntry, vertexShader ? L"vs_6_0" : L"ps_6_0", bytecode, error);
		}
		Microsoft::WRL::ComPtr<ID3DBlob> legacyBytecode;
		if (!LegacyHlslCompiler::CompileFile(file, entry.c_str(), vertexShader ? "vs_5_1" : "ps_5_1",
			legacyBytecode, error))
			return false;
		bytecode.resize(legacyBytecode->GetBufferSize());
		std::memcpy(bytecode.data(), legacyBytecode->GetBufferPointer(), bytecode.size());
		return true;
	}
}
