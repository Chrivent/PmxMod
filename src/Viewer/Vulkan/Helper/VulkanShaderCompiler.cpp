#include "VulkanShaderCompiler.h"

#include "../../GlslPreprocessor.h"

#include <shaderc/shaderc.hpp>

namespace Chrivent {
	std::string VulkanShaderCompiler::BuildPreamble() {
		return R"(#version 460
#define PMX_MODEL_VERTEX_CONSTANTS layout(std140, set = 0, binding = 0) uniform ModelVertexConstants { mat4 wv; mat4 wvp; } vertexConstants;
#define PMX_MODEL_PIXEL_CONSTANTS layout(std140, set = 1, binding = 0) uniform ModelPixelConstants { vec4 diffuseAlpha; vec4 ambientSpecularPower; vec4 specular; vec4 lightColor; vec4 lightDir; vec4 texMulFactor; vec4 texAddFactor; vec4 toonTexMulFactor; vec4 toonTexAddFactor; vec4 sphereTexMulFactor; vec4 sphereTexAddFactor; ivec4 textureModes; } pixelConstants;
#define PMX_MODEL_TEX layout(set = 2, binding = 0) uniform sampler2D tex;
#define PMX_MODEL_TOON_TEX layout(set = 2, binding = 1) uniform sampler2D toonTex;
#define PMX_MODEL_SPHERE_TEX layout(set = 2, binding = 2) uniform sampler2D sphereTex;
#define PMX_EDGE_VERTEX_CONSTANTS layout(std140, set = 0, binding = 0) uniform EdgeVertexConstants { mat4 wv; mat4 wvp; vec2 screenSize; float edgeSize; } edgeConstants;
#define PMX_EDGE_PIXEL_CONSTANTS layout(std140, set = 1, binding = 0) uniform EdgePixelConstants { vec4 edgeColor; } edgeConstants;
#define PMX_GROUND_SHADOW_VERTEX_CONSTANTS layout(std140, set = 0, binding = 0) uniform GroundShadowVertexConstants { mat4 wvp; } shadowConstants;
#define PMX_GROUND_SHADOW_PIXEL_CONSTANTS layout(std140, set = 1, binding = 0) uniform GroundShadowPixelConstants { vec4 shadowColor; } shadowConstants;
)";
	}

	bool VulkanShaderCompiler::ReadShaderFile(
		const std::filesystem::path& file,
		std::string& outCode,
		std::string& outError) {
		return GlslPreprocessor::LoadSource(file, BuildPreamble(), outCode, outError);
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
}
