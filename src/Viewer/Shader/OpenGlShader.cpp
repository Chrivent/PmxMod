#include "Viewer/Shader/OpenGlShader.h"

#include "Viewer/Error/OpenGlErrorState.h"
#include "Viewer/Shader/DxcHlslCompiler.h"
#include <spirv_cross/spirv_glsl.hpp>
#include <utility>

namespace Chrivent {
	GraphicsError::Result<std::string> OpenGlProgramBuilder::ConvertSpirvToGlsl(
		const std::vector<uint32_t>& code) {
		try {
			spirv_cross::CompilerGLSL compiler(code);
			spirv_cross::CompilerGLSL::Options options;
			options.version = 460;
			options.es = false;
			compiler.set_common_options(options);
			compiler.build_dummy_sampler_for_combined_images();
			compiler.build_combined_image_samplers();
			for (const auto& sampler : compiler.get_combined_image_samplers()) {
				if (!compiler.has_decoration(sampler.image_id, spv::DecorationBinding))
					continue;
				compiler.set_decoration(sampler.combined_id, spv::DecorationBinding,
					compiler.get_decoration(sampler.image_id, spv::DecorationBinding));
			}
			return compiler.compile();
		} catch (const spirv_cross::CompilerError& compilerError) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::EffectConfigurationFailed, "SPIR-V를 GLSL로 변환",
				"SPIR-V를 OpenGL GLSL로 변환하지 못했습니다: " + std::string(compilerError.what())));
		}
	}

	std::string OpenGlProgramBuilder::ReadShaderLog(const GLuint shader) {
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength <= 1)
			return {};
		std::string log(logLength, '\0');
		glGetShaderInfoLog(shader, logLength, nullptr, log.data());
		return log;
	}

	std::string OpenGlProgramBuilder::ReadProgramLog(const GLuint program) {
		GLint logLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength <= 1)
			return {};
		std::string log(logLength, '\0');
		glGetProgramInfoLog(program, logLength, nullptr, log.data());
		return log;
	}

	GraphicsError::Result<GLuint> OpenGlProgramBuilder::CreateStage(
		const GLenum shaderType, const std::vector<uint32_t>& code, const std::string& entry) {
		auto sourceResult = ConvertSpirvToGlsl(code);
		if (!sourceResult)
			return std::unexpected(sourceResult.error());
		const std::string& source = *sourceResult;
		OpenGlErrorState::Clear();
		const GLuint shader = glCreateShader(shaderType);
		const GLenum result = OpenGlErrorState::Take();
		if (shader == 0 || result != GL_NO_ERROR) {
			if (shader != 0)
				glDeleteShader(shader);
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
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
		return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
			GraphicsErrorCode::EffectConfigurationFailed, "GLSL 셰이더 컴파일", std::move(message)));
	}

	GraphicsError::Result<GLuint> OpenGlProgramBuilder::CreateProgram(
		const std::filesystem::path& shaderFile, const std::string& vertexEntry,
		const std::string& pixelEntry, const SpirvBindingProfile bindingProfile,
		const bool invertVertexY) {
		const std::wstring wideVertexEntry(vertexEntry.begin(), vertexEntry.end());
		const std::wstring widePixelEntry(pixelEntry.begin(), pixelEntry.end());
		auto vertexCodeResult = DxcHlslCompiler::CompileSpirv(
			shaderFile, wideVertexEntry, L"vs_6_0",
			SpirvTarget::OpenGl, bindingProfile, invertVertexY);
		if (!vertexCodeResult) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::EffectConfigurationFailed, "vertex SPIR-V 셰이더 컴파일",
				std::move(vertexCodeResult.error().message)));
		}
		auto pixelCodeResult = DxcHlslCompiler::CompileSpirv(
			shaderFile, widePixelEntry, L"ps_6_0",
			SpirvTarget::OpenGl, bindingProfile);
		if (!pixelCodeResult) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
				GraphicsErrorCode::EffectConfigurationFailed, "pixel SPIR-V 셰이더 컴파일",
				std::move(pixelCodeResult.error().message)));
		}
		const std::vector<uint32_t>& vertexCode = *vertexCodeResult;
		const std::vector<uint32_t>& pixelCode = *pixelCodeResult;
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
		OpenGlErrorState::Clear();
		const GLuint program = glCreateProgram();
		const GLenum result = OpenGlErrorState::Take();
		if (program == 0 || result != GL_NO_ERROR) {
			if (program != 0)
				glDeleteProgram(program);
			glDeleteShader(vertexShader);
			glDeleteShader(pixelShader);
			return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
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
		return std::unexpected(GraphicsError::Create(GraphicsApi::OpenGl,
			GraphicsErrorCode::EffectConfigurationFailed, "shader program 링크", std::move(message)));
	}

	OpenGlShaderProgram::~OpenGlShaderProgram() {
		Reset();
	}

	OpenGlShaderProgram::OpenGlShaderProgram(OpenGlShaderProgram&& other) noexcept :
		program(std::exchange(other.program, 0)) {}

	OpenGlShaderProgram& OpenGlShaderProgram::operator=(OpenGlShaderProgram&& other) noexcept {
		if (this == &other)
			return *this;
		Reset();
		program = std::exchange(other.program, 0);
		return *this;
	}

	void OpenGlShaderProgram::Reset() {
		if (program != 0)
			glDeleteProgram(program);
		program = 0;
	}

	GraphicsError::Result<void> OpenGlShaderProgram::InitializeProgram(const ShaderProgramDefinition& shaderProgram,
		const SpirvBindingProfile bindingProfile, const bool invertVertexY) {
		const auto result = OpenGlProgramBuilder::CreateProgram(shaderProgram.shaderPath,
			shaderProgram.vertexEntry, shaderProgram.pixelEntry, bindingProfile, invertVertexY);
		if (!result)
			return std::unexpected(result.error());
		Reset();
		program = *result;
		return {};
	}

	GraphicsError::Result<void> OpenGlSceneShaderProgram::Initialize(
		const ShaderProgramDefinition& shaderProgram) {
		return InitializeProgram(shaderProgram, SpirvBindingProfile::Scene);
	}

	GraphicsError::Result<void> OpenGlPostProcessShaderProgram::Initialize(
		const ShaderProgramDefinition& shaderProgram) {
		return InitializeProgram(shaderProgram, SpirvBindingProfile::PostProcess, true);
	}
}
