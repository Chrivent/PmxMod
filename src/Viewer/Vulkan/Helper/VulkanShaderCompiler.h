#pragma once

#include "VulkanDevice.h"

#include <filesystem>

namespace Chrivent {
	class VulkanShaderCompiler {
		// SPIR-V 셰이더 파일을 바이너리 데이터로 읽는다.
		static bool ReadSpvFile(const std::filesystem::path& file, std::vector<char>& outSpv, std::string& outError);
		// Vulkan shader stage를 로그에 출력할 문자열로 변환한다.
		static const char* ShaderStageName(VkShaderStageFlagBits shaderStage);

	public:
		// Vulkan 셰이더 파일을 SPIR-V 바이트 코드로 준비한다.
		static bool CompileFile(
			const std::filesystem::path& file,
			VkShaderStageFlagBits shaderStage,
			std::vector<char>& outSpv,
			std::string& outError);
	};
}
