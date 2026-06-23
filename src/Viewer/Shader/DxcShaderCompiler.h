#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace Chrivent {
	enum class SpirvTarget {
		Vulkan,
		OpenGl
	};

	class DxcShaderCompiler {
		// 공통 DXC 호출을 수행하고 컴파일된 객체 바이트를 반환한다.
		static bool CompileObject(const std::filesystem::path& file, const std::wstring& entry,
			const std::wstring& target, std::span<const wchar_t* const> additionalArguments,
			std::vector<uint8_t>& outObject, std::string& outError);

	public:
		// HLSL 파일의 지정한 진입점을 DXIL 바이트 코드로 컴파일한다.
		static bool CompileDxil(const std::filesystem::path& file, const std::wstring& entry,
			const std::wstring& target, std::vector<uint8_t>& outDxil, std::string& outError);
		// HLSL 파일의 지정한 진입점을 SPIR-V 바이트 코드로 컴파일한다.
		static bool CompileSpirv(const std::filesystem::path& file, const std::wstring& entry,
			const std::wstring& target, SpirvTarget spirvTarget, std::vector<uint32_t>& outSpirv,
			std::string& outError, bool invertVertexY = false);
	};
}
