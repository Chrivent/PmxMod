#pragma once

#include "VulkanDevice.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Chrivent {
	class VulkanShaderCompiler {
		// Vulkan GLSL 공통 preamble을 만든다.
		static std::string BuildPreamble();
		// GLSL 셰이더 파일을 읽어 전처리된 최종 소스를 만든다.
		static bool ReadShaderFile(const std::filesystem::path& file, std::string& outCode, std::string& outError);
		// Vulkan shader stage를 로그에 출력할 문자열로 변환한다.
		static const char* ShaderStageName(VkShaderStageFlagBits shaderStage);
		// Vulkan shader stage를 shaderc shader kind로 변환한다.
		static int ShaderKind(VkShaderStageFlagBits shaderStage);

	public:
		// Vulkan GLSL 셰이더 파일을 SPIR-V 바이트 코드로 컴파일한다.
		static bool CompileFile(
			const std::filesystem::path& file,
			VkShaderStageFlagBits shaderStage,
			std::vector<uint32_t>& outSpv,
			std::string& outError);
	};
}
