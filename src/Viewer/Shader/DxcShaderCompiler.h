#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Chrivent {
	enum class SpirvTarget {
		Vulkan,
		OpenGl
	};

	class DxcShaderCompiler {
	public:
		// HLSL 파일의 지정한 진입점을 SPIR-V 바이트 코드로 컴파일한다.
		static bool CompileSpirv(const std::filesystem::path& file, const std::wstring& entry,
			const std::wstring& target, SpirvTarget spirvTarget, std::vector<uint32_t>& outSpirv, std::string& outError);
	};
}
