#pragma once

#include <d3dcompiler.h>
#include <filesystem>
#include <string>
#include <wrl/client.h>

namespace Chrivent {
	// 레거시 D3DCompiler로 Shader Model 5 HLSL 바이트 코드를 생성한다.
	class LegacyHlslCompiler {
	public:
		// HLSL 파일을 Shader Model 5 계열 바이트 코드로 컴파일한다.
		static bool CompileFile(const std::filesystem::path& file, const char* entry, const char* target,
			Microsoft::WRL::ComPtr<ID3DBlob>& outBytecode, std::string& outError);
	};
}
