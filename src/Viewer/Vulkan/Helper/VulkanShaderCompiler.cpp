#include "VulkanShaderCompiler.h"

#include <fstream>

namespace Chrivent {
	bool VulkanShaderCompiler::CompileFile(
		const std::filesystem::path& file,
		const VkShaderStageFlagBits shaderStage,
		std::vector<char>& outSpv,
		std::string& outError) {
		if (file.extension() == ".spv")
			return ReadSpvFile(file, outSpv, outError);
		outError = "Runtime GLSL compile is not configured for Vulkan "
			+ std::string(ShaderStageName(shaderStage))
			+ " shader: " + file.string();
		return false;
	}

	bool VulkanShaderCompiler::ReadSpvFile(
		const std::filesystem::path& file,
		std::vector<char>& outSpv,
		std::string& outError) {
		std::ifstream stream(file, std::ios::ate | std::ios::binary);
		if (!stream) {
			outError = "Failed to open SPIR-V shader file: " + file.string();
			return false;
		}
		const std::streamsize fileSize = stream.tellg();
		if (fileSize <= 0) {
			outError = "SPIR-V shader file is empty: " + file.string();
			return false;
		}
		outSpv.resize(fileSize);
		stream.seekg(0);
		if (!stream.read(outSpv.data(), fileSize)) {
			outError = "Failed to read SPIR-V shader file: " + file.string();
			return false;
		}
		return true;
	}

	const char* VulkanShaderCompiler::ShaderStageName(const VkShaderStageFlagBits shaderStage) {
		switch (shaderStage) {
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "vertex";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "fragment";
		default:
			return "unknown";
		}
	}
}
