#include "Viewer/Shader/OpenGlShader.h"

#include "Viewer/Shader/DxcHlslCompiler.h"

#include <spirv_cross/spirv_glsl.hpp>
#include <utility>

namespace Chrivent {
	std::string OpenGlShaderBuilder::ReadShaderLog(const GLuint shader) {
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength <= 1)
			return {};
		std::string log(logLength, '\0');
		glGetShaderInfoLog(shader, logLength, nullptr, log.data());
		return log;
	}

	std::string OpenGlShaderBuilder::ReadProgramLog(const GLuint program) {
		GLint logLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength <= 1)
			return {};
		std::string log(logLength, '\0');
		glGetProgramInfoLog(program, logLength, nullptr, log.data());
		return log;
	}

	GraphicsResult<GLuint> OpenGlShaderBuilder::CreateStage(
		const GLenum shaderType, const std::vector<uint32_t>& code, const std::string& entry) {
		std::string source;
		try {
			spirv_cross::CompilerGLSL compiler(code);
			spirv_cross::CompilerGLSL::Options options;
			options.version = 460;
			options.es = false;
			compiler.set_common_options(options);
			compiler.build_combined_image_samplers();
			for (const auto& sampler : compiler.get_combined_image_samplers()) {
				if (!compiler.has_decoration(sampler.image_id, spv::DecorationBinding))
					continue;
				compiler.set_decoration(sampler.combined_id, spv::DecorationBinding,
					compiler.get_decoration(sampler.image_id, spv::DecorationBinding));
			}
			source = compiler.compile();
		} catch (const spirv_cross::CompilerError& compilerError) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::EffectConfigurationFailed, "SPIR-V를 GLSL로 변환",
				"SPIR-V를 OpenGL GLSL로 변환하지 못했습니다: " + std::string(compilerError.what())));
		}
		const GLuint shader = glCreateShader(shaderType);
		if (shader == 0) {
			const GLenum result = glGetError();
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "shader 객체 생성",
				"OpenGL shader 객체를 만들지 못했습니다", result, result != GL_NO_ERROR));
		}
		const char* sourceData = source.c_str();
		const GLint sourceSize = static_cast<GLint>(source.size());
		glShaderSource(shader, 1, &sourceData, &sourceSize);
		glCompileShader(shader);
		GLint compileStatus = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
		if (compileStatus == GL_TRUE)
			return shader;
		std::string message = "생성된 OpenGL GLSL을 컴파일하지 못했습니다: entry=" + entry;
		const std::string log = ReadShaderLog(shader);
		if (!log.empty()) {
			message += '\n';
			message += log;
		}
		glDeleteShader(shader);
		return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
			GraphicsErrorCode::EffectConfigurationFailed, "GLSL 셰이더 컴파일", std::move(message)));
	}

	GraphicsResult<GLuint> OpenGlShaderBuilder::CreateShader(
		const std::filesystem::path& shaderFile, const std::string& vertexEntry,
		const std::string& pixelEntry, const SpirvBindingProfile bindingProfile,
		const bool invertVertexY) {
		std::vector<uint32_t> vertexCode;
		std::vector<uint32_t> pixelCode;
		const std::wstring wideVertexEntry(vertexEntry.begin(), vertexEntry.end());
		const std::wstring widePixelEntry(pixelEntry.begin(), pixelEntry.end());
		std::string error;
		if (!DxcHlslCompiler::CompileSpirv(shaderFile, wideVertexEntry, L"vs_6_0",
			SpirvTarget::OpenGl, bindingProfile, vertexCode, error, invertVertexY)
			|| !DxcHlslCompiler::CompileSpirv(shaderFile, widePixelEntry, L"ps_6_0",
				SpirvTarget::OpenGl, bindingProfile, pixelCode, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::EffectConfigurationFailed, "SPIR-V 셰이더 컴파일",
				error.empty() ? "OpenGL용 SPIR-V 셰이더를 컴파일하지 못했습니다" : std::move(error)));
		}
		const auto vertexShaderResult = CreateStage(GL_VERTEX_SHADER, vertexCode, vertexEntry);
		if (!vertexShaderResult)
			return std::unexpected(vertexShaderResult.error());
		const GLuint vertexShader = *vertexShaderResult;
		const auto pixelShaderResult = CreateStage(GL_FRAGMENT_SHADER, pixelCode, pixelEntry);
		if (!pixelShaderResult) {
			glDeleteShader(vertexShader);
			return std::unexpected(pixelShaderResult.error());
		}
		const GLuint pixelShader = *pixelShaderResult;
		const GLuint program = glCreateProgram();
		if (program == 0) {
			const GLenum result = glGetError();
			glDeleteShader(vertexShader);
			glDeleteShader(pixelShader);
			return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
				GraphicsErrorCode::ResourceCreationFailed, "shader program 생성",
				"OpenGL shader program을 만들지 못했습니다", result, result != GL_NO_ERROR));
		}
		glAttachShader(program, vertexShader);
		glAttachShader(program, pixelShader);
		glLinkProgram(program);
		glDeleteShader(vertexShader);
		glDeleteShader(pixelShader);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus == GL_TRUE)
			return program;
		std::string message = "OpenGL SPIR-V shader program을 link하지 못했습니다: "
			+ shaderFile.string();
		const std::string log = ReadProgramLog(program);
		if (!log.empty()) {
			message += '\n';
			message += log;
		}
		glDeleteProgram(program);
		return std::unexpected(MakeGraphicsError(GraphicsApi::OpenGl,
			GraphicsErrorCode::EffectConfigurationFailed, "shader program 링크", std::move(message)));
	}

	OpenGlShader::~OpenGlShader() {
		if (program != 0)
			glDeleteProgram(program);
		program = 0;
	}

	GraphicsResult<void> OpenGlModelShader::Initialize(
		const ShaderProgramDefinition& shaderProgram) {
		const auto result = OpenGlShaderBuilder::CreateShader(shaderProgram.shaderPath,
			shaderProgram.vertexEntry, shaderProgram.pixelEntry, SpirvBindingProfile::Scene);
		if (!result)
			return std::unexpected(result.error());
		program = *result;
		return {};
	}

	GraphicsResult<void> OpenGlEdgeShader::Initialize(
		const ShaderProgramDefinition& shaderProgram) {
		const auto result = OpenGlShaderBuilder::CreateShader(shaderProgram.shaderPath,
			shaderProgram.vertexEntry, shaderProgram.pixelEntry, SpirvBindingProfile::Scene);
		if (!result)
			return std::unexpected(result.error());
		program = *result;
		return {};
	}

	GraphicsResult<void> OpenGlGroundShadowShader::Initialize(
		const ShaderProgramDefinition& shaderProgram) {
		const auto result = OpenGlShaderBuilder::CreateShader(shaderProgram.shaderPath,
			shaderProgram.vertexEntry, shaderProgram.pixelEntry, SpirvBindingProfile::Scene);
		if (!result)
			return std::unexpected(result.error());
		program = *result;
		return {};
	}

	GraphicsResult<void> OpenGlDepthOnlyShader::Initialize(
		const ShaderProgramDefinition& shaderProgram) {
		const auto result = OpenGlShaderBuilder::CreateShader(shaderProgram.shaderPath,
			shaderProgram.vertexEntry, shaderProgram.pixelEntry, SpirvBindingProfile::Scene);
		if (!result)
			return std::unexpected(result.error());
		program = *result;
		return {};
	}

	GraphicsResult<void> OpenGlSceneVelocityShader::Initialize(
		const ShaderProgramDefinition& shaderProgram) {
		const auto result = OpenGlShaderBuilder::CreateShader(shaderProgram.shaderPath,
			shaderProgram.vertexEntry, shaderProgram.pixelEntry, SpirvBindingProfile::Scene);
		if (!result)
			return std::unexpected(result.error());
		program = *result;
		return {};
	}

	GraphicsResult<void> OpenGlPostProcessShader::Initialize(
		const ShaderProgramDefinition& shaderProgram) {
		const auto result = OpenGlShaderBuilder::CreateShader(shaderProgram.shaderPath,
			shaderProgram.vertexEntry, shaderProgram.pixelEntry,
			SpirvBindingProfile::PostProcess, true);
		if (!result)
			return std::unexpected(result.error());
		program = *result;
		return {};
	}
}
