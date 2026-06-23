#include "Viewer/Shader/ShaderCompiler.h"

namespace Chrivent {
	bool ShaderCompiler::CompileFile(const std::filesystem::path& file, const char* entry, const char* target,
		Microsoft::WRL::ComPtr<ID3DBlob>& outBytecode, std::string& outError) {
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		const HRESULT result = D3DCompileFromFile(file.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry,
			target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &outBytecode, &errorBlob);
		if (SUCCEEDED(result))
			return true;
		outError = "Failed to compile shader: " + file.string() + " entry=" + entry + " target=" + target + '\n';
		if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr)
			outError += static_cast<const char*>(errorBlob->GetBufferPointer());
		return false;
	}
}
