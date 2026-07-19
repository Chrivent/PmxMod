#pragma once

#include "Viewer/Shader/ShaderCompileError.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <filesystem>
#include <span>
#include <vector>

namespace Chrivent {
	enum class SpirvTarget {
		Vulkan,
		OpenGl
	};

	// DXC를 통해 HLSL을 DXIL 또는 API별 SPIR-V로 컴파일한다.
	class DxcHlslCompiler {
		// DXC를 호출하고 컴파일된 객체 바이트를 반환한다.
		static ShaderCompileError::Result<std::vector<uint8_t>> CompileObject(
			const std::filesystem::path& file, const std::wstring& entry,
			const std::wstring& target, std::span<const wchar_t* const> additionalArguments);

	public:
		// HLSL 파일의 지정한 진입점을 DXIL 바이트 코드로 컴파일한다.
		static ShaderCompileError::Result<std::vector<uint8_t>> CompileDxil(
			const std::filesystem::path& file, const std::wstring& entry, const std::wstring& target);
		// HLSL 파일의 지정한 진입점을 SPIR-V 바이트 코드로 컴파일한다.
		static ShaderCompileError::Result<std::vector<uint32_t>> CompileSpirv(
			const std::filesystem::path& file, const std::wstring& entry,
			const std::wstring& target, SpirvTarget spirvTarget, SpirvBindingProfile bindingProfile,
			bool invertVertexY = false);
	};
}
