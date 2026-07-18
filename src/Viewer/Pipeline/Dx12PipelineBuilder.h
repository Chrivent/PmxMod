#pragma once

#include "Viewer/Device/Dx12Device.h"

#include <d3d12.h>
#include <filesystem>
#include <string>
#include <vector>
#include <wrl/client.h>

namespace Chrivent {
	// D3D12 파이프라인 생성에 필요한 공통 설정과 셰이더 컴파일을 제공한다.
	class Dx12PipelineBuilder {
	public:
		// Root Signature를 직렬화하고 DX12 리소스로 생성한다.
		static bool CreateRootSignature(const Dx12Device& sourceDevice,
			const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc,
			Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature, std::string& error);
		// 공통 rasterizer 기본값을 채우고 패스별 cull mode를 반영한다.
		static void ConfigureRasterizer(D3D12_RASTERIZER_DESC& rasterizerDesc, D3D12_CULL_MODE cullMode);
		// 디바이스 Shader Model에 맞춰 DXC 또는 레거시 컴파일러로 셰이더를 만든다.
		static bool CompileShader(const Dx12Device& sourceDevice, const std::filesystem::path& file,
			const std::string& entry, bool vertexShader, std::vector<uint8_t>& bytecode, std::string& error);
	};
}
