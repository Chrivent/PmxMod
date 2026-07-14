#include "Viewer/Shader/OpenGlShader.h"

#include "Viewer/Shader/ModernHlslCompiler.h"

#include <iostream>
#include <spirv_cross/spirv_glsl.hpp>

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

	GLuint OpenGlShaderBuilder::CreateStage(const GLenum shaderType, const std::vector<uint32_t>& code,
		const std::string& entry) {
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
		} catch (const spirv_cross::CompilerError& error) {
			std::cerr << "Failed to convert SPIR-V to OpenGL GLSL: " << error.what() << '\n';
			return 0;
		}
		const GLuint shader = glCreateShader(shaderType);
		if (shader == 0)
			return 0;
		const char* sourceData = source.c_str();
		const GLint sourceSize = static_cast<GLint>(source.size());
		glShaderSource(shader, 1, &sourceData, &sourceSize);
		glCompileShader(shader);
		GLint compileStatus = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
		if (compileStatus == GL_TRUE)
			return shader;
		std::cerr << "Failed to compile generated OpenGL GLSL for " << entry << ".\n";
		const std::string log = ReadShaderLog(shader);
		if (!log.empty())
			std::cerr << log << '\n';
		glDeleteShader(shader);
		return 0;
	}

	GLuint OpenGlShaderBuilder::CreateShader(const std::filesystem::path& shaderFile, const std::string& vertexEntry,
		const std::string& pixelEntry, const bool invertVertexY) {
		std::vector<uint32_t> vertexCode;
		std::vector<uint32_t> pixelCode;
		std::string error;
		const std::wstring wideVertexEntry(vertexEntry.begin(), vertexEntry.end());
		const std::wstring widePixelEntry(pixelEntry.begin(), pixelEntry.end());
		if (!ModernHlslCompiler::CompileSpirv(shaderFile, wideVertexEntry, L"vs_6_0",
			SpirvTarget::OpenGl, vertexCode, error, invertVertexY)
			|| !ModernHlslCompiler::CompileSpirv(shaderFile, widePixelEntry, L"ps_6_0",
				SpirvTarget::OpenGl, pixelCode, error)) {
			std::cerr << error << '\n';
			return 0;
		}
		const GLuint vertexShader = CreateStage(GL_VERTEX_SHADER, vertexCode, vertexEntry);
		const GLuint pixelShader = CreateStage(GL_FRAGMENT_SHADER, pixelCode, pixelEntry);
		if (vertexShader == 0 || pixelShader == 0) {
			if (vertexShader != 0)
				glDeleteShader(vertexShader);
			if (pixelShader != 0)
				glDeleteShader(pixelShader);
			return 0;
		}
		const GLuint program = glCreateProgram();
		if (program == 0) {
			glDeleteShader(vertexShader);
			glDeleteShader(pixelShader);
			return 0;
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
		std::cerr << "Failed to link OpenGL SPIR-V shader program: " << shaderFile.string() << '\n';
		const std::string log = ReadProgramLog(program);
		if (!log.empty())
			std::cerr << log << '\n';
		glDeleteProgram(program);
		return 0;
	}

	GLuint OpenGlShaderBuilder::CreateVertexOnlyShader(const std::filesystem::path& shaderFile,
		const std::string& vertexEntry) {
		std::vector<uint32_t> vertexCode;
		std::string error;
		const std::wstring wideVertexEntry(vertexEntry.begin(), vertexEntry.end());
		if (!ModernHlslCompiler::CompileSpirv(shaderFile, wideVertexEntry, L"vs_6_0",
			SpirvTarget::OpenGl, vertexCode, error)) {
			std::cerr << error << '\n';
			return 0;
		}
		const GLuint vertexShader = CreateStage(GL_VERTEX_SHADER, vertexCode, vertexEntry);
		if (vertexShader == 0)
			return 0;
		const GLuint program = glCreateProgram();
		if (program == 0) {
			glDeleteShader(vertexShader);
			return 0;
		}
		glAttachShader(program, vertexShader);
		glLinkProgram(program);
		glDeleteShader(vertexShader);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus == GL_TRUE)
			return program;
		std::cerr << "Failed to link OpenGL vertex-only shader program: " << shaderFile.string() << '\n';
		const std::string log = ReadProgramLog(program);
		if (!log.empty())
			std::cerr << log << '\n';
		glDeleteProgram(program);
		return 0;
	}
	
    OpenGlShader::~OpenGlShader() {
        if (program != 0)
            glDeleteProgram(program);
        program = 0;
    }

    bool OpenGlModelShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderBuilder::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        normalLocation = 1;
        uvLocation = 2;
        return true;
    }

    bool OpenGlEdgeShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderBuilder::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        normalLocation = 1;
        return true;
    }

    bool OpenGlGroundShadowShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderBuilder::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        return true;
    }

    bool OpenGlDepthOnlyShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderBuilder::CreateVertexOnlyShader(pass.shaderPath, pass.vertexEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        normalLocation = 1;
        uvLocation = 2;
        return true;
    }

	bool OpenGlSceneVelocityShader::Initialize(const EffectPassDefinition& pass) {
		program = OpenGlShaderBuilder::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
		if (program == 0)
			return false;
		positionLocation = 0;
		previousPositionLocation = 1;
		return true;
	}

    bool OpenGlPostProcessShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderBuilder::CreateShader(pass.shaderPath, pass.vertexEntry,
            pass.pixelEntry, true);
        return program != 0;
    }
}
