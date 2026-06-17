#pragma once

#include <d3dcompiler.h>
#include <filesystem>
#include <string>
#include <wrl/client.h>

namespace Chrivent {
	class HlslShaderCompiler {
	public:
		// HLSL 파일을 지정한 entry/target으로 컴파일해 bytecode blob을 만든다.
		static bool CompileFile(
			const std::filesystem::path& file,
			const char* entry,
			const char* target,
			Microsoft::WRL::ComPtr<ID3DBlob>& outBytecode,
			std::string& outError);
	};
}
