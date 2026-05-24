#include "VulkanShaderCompiler.h"

#include <fstream>
#include <shaderc/shaderc.hpp>

namespace Chrivent {
	bool VulkanShaderCompiler::CompileFile(
		const std::filesystem::path& file,
		const VkShaderStageFlagBits shaderStage,
		std::vector<uint32_t>& outSpv,
		std::string& outError) {
		std::string code;
		if (!ReadShaderFile(file, code, outError))
			return false;
		const shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetOptimizationLevel(shaderc_optimization_level_performance);
		const shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
			code,
			static_cast<shaderc_shader_kind>(ShaderKind(shaderStage)),
			file.string().c_str(),
			options);
		if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
			outError = "Failed to compile Vulkan GLSL "
				+ std::string(ShaderStageName(shaderStage))
				+ " shader: " + file.string() + '\n'
				+ result.GetErrorMessage();
			return false;
		}
		outSpv.assign(result.cbegin(), result.cend());
		return true;
	}

	bool VulkanShaderCompiler::ReadShaderFile(
		const std::filesystem::path& file,
		std::string& outCode,
		std::string& outError) {
		std::ifstream stream(file);
		if (!stream) {
			outError = "Failed to open Vulkan GLSL shader file: " + file.string();
			return false;
		}
		outCode.assign(std::istreambuf_iterator(stream), {});
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

	int VulkanShaderCompiler::ShaderKind(const VkShaderStageFlagBits shaderStage) {
		switch (shaderStage) {
		case VK_SHADER_STAGE_VERTEX_BIT:
			return shaderc_glsl_vertex_shader;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return shaderc_glsl_fragment_shader;
		default:
			return shaderc_glsl_infer_from_source;
		}
	}
}
