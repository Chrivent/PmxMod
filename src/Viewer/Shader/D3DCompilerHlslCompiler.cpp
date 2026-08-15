#include "Viewer/Shader/D3DCompilerHlslCompiler.h"

#include <fstream>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Chrivent {
	// 열어 둔 include 파일의 내용과 다음 상대 경로 기준을 함께 보관한다.
	struct D3DCompilerIncludeSource {
		std::vector<char> bytes;
		std::filesystem::path directory;
	};

	// 중첩 HLSL include를 현재 include 파일의 디렉터리 기준으로 해석한다.
	class D3DCompilerIncludeHandler final : public ID3DInclude {
		std::filesystem::path rootDirectory;
		std::unordered_map<const void*, std::unique_ptr<D3DCompilerIncludeSource>> sources;

	public:
		// 최상위 셰이더 파일의 디렉터리를 첫 include 탐색 기준으로 설정한다.
		explicit D3DCompilerIncludeHandler(const std::filesystem::path& shaderFile)
			: rootDirectory(shaderFile.parent_path()) {}

		// include 파일을 열고 컴파일러가 Close를 호출할 때까지 내용을 유지한다.
		HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR fileName, LPCVOID parentData,
			LPCVOID* data, UINT* byteCount) override {
			if (fileName == nullptr || data == nullptr || byteCount == nullptr)
				return E_INVALIDARG;
			std::filesystem::path directory = rootDirectory;
			if (parentData != nullptr) {
				const auto parent = sources.find(parentData);
				if (parent != sources.end())
					directory = parent->second->directory;
			}
			std::filesystem::path includePath(
				std::u8string(reinterpret_cast<const char8_t*>(fileName)));
			if (includePath.is_relative())
				includePath = directory / includePath;
			includePath = includePath.lexically_normal();
			std::ifstream stream(includePath, std::ios::binary | std::ios::ate);
			if (!stream)
				return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
			const std::streampos endPosition = stream.tellg();
			if (endPosition < 0 || static_cast<uint64_t>(endPosition) > std::numeric_limits<UINT>::max())
				return E_FAIL;
			const size_t size = endPosition;
			auto source = std::make_unique<D3DCompilerIncludeSource>();
			source->bytes.resize(size + 1);
			source->directory = includePath.parent_path();
			stream.seekg(0);
			if (size > 0 && !stream.read(source->bytes.data(), size))
				return E_FAIL;
			source->bytes[size] = '\0';
			*data = source->bytes.data();
			*byteCount = static_cast<UINT>(size);
			sources.emplace(*data, std::move(source));
			return S_OK;
		}

		// 컴파일러가 사용을 끝낸 include 파일 내용을 해제한다.
		HRESULT STDMETHODCALLTYPE Close(const LPCVOID data) override {
			return sources.erase(data) == 1 ? S_OK : E_INVALIDARG;
		}
	};

	ShaderCompileError::Result<Microsoft::WRL::ComPtr<ID3DBlob>> D3DCompilerHlslCompiler::CompileFile(
		const std::filesystem::path& file, const char* entry, const char* target) {
		D3DCompilerIncludeHandler includeHandler(file);
		Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
		const HRESULT result = D3DCompileFromFile(file.c_str(), nullptr, &includeHandler, entry,
			target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &bytecode, &errorBlob);
		if (SUCCEEDED(result))
			return bytecode;
		std::string message = "셰이더를 컴파일하지 못했습니다: " + file.string()
			+ " entry=" + entry + " target=" + target + '\n';
		if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr)
			message += static_cast<const char*>(errorBlob->GetBufferPointer());
		return std::unexpected(ShaderCompileError{
			.shaderPath = file,
			.message = std::move(message)
		});
	}
}
