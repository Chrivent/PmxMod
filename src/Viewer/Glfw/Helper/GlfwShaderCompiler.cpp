#include "GlfwShaderCompiler.h"

#include "../../GlslPreprocessor.h"

#include <iostream>

namespace Chrivent {
	std::string GlfwShaderCompiler::BuildPreamble() {
		return R"(#version 460 core
#define PMX_LAYOUT_UBO(setIndex, bindingIndex) layout(std140, binding = bindingIndex)
#define PMX_LAYOUT_SAMPLER(setIndex, bindingIndex) layout(binding = bindingIndex)
)";
	}

	const char* GlfwShaderCompiler::ShaderTypeName(const GLenum shaderType) {
		switch (shaderType) {
		case GL_VERTEX_SHADER:
			return "vertex";
		case GL_FRAGMENT_SHADER:
			return "fragment";
		default:
			return "unknown";
		}
	}

	std::string GlfwShaderCompiler::GetShaderInfoLog(const GLuint shader) {
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength <= 1)
			return {};
		std::string log(logLength, '\0');
		glGetShaderInfoLog(shader, logLength, nullptr, log.data());
		return log;
	}

	std::string GlfwShaderCompiler::GetProgramInfoLog(const GLuint program) {
		GLint logLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength <= 1)
			return {};
		std::string log(logLength, '\0');
		glGetProgramInfoLog(program, logLength, nullptr, log.data());
		return log;
	}

	bool GlfwShaderCompiler::ReadShaderFile(const std::filesystem::path& file, std::string& code) {
		std::string error;
		if (!GlslPreprocessor::LoadSource(file, BuildPreamble(), code, error)) {
			std::cerr << error << '\n';
			return false;
		}
		return true;
	}

	GLuint GlfwShaderCompiler::CompileShader(const GLenum shaderType, const std::string& code) {
		const GLuint shader = glCreateShader(shaderType);
		if (!shader)
			return 0;
		const char* codes = code.c_str();
		const auto codesLen = static_cast<GLint>(code.size());
		glShaderSource(shader, 1, &codes, &codesLen);
		glCompileShader(shader);
		GLint compileStatus = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
		if (compileStatus == GL_FALSE) {
			std::cerr << "Failed to compile GLSL " << ShaderTypeName(shaderType) << " shader.\n";
			const std::string log = GetShaderInfoLog(shader);
			if (!log.empty())
				std::cerr << log << '\n';
			glDeleteShader(shader);
			return 0;
		}
		return shader;
	}

	GLuint GlfwShaderCompiler::CreateShader(
		const std::filesystem::path& vertexFile,
		const std::filesystem::path& fragmentFile) {
		std::string vsCode;
		std::string fsCode;
		if (!ReadShaderFile(vertexFile, vsCode) || !ReadShaderFile(fragmentFile, fsCode))
			return 0;
		const GLuint vs = CompileShader(GL_VERTEX_SHADER, vsCode);
		const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsCode);
		if (!vs || !fs) {
			if (vs)
				glDeleteShader(vs);
			if (fs)
				glDeleteShader(fs);
			return 0;
		}
		const GLuint prog = glCreateProgram();
		if (prog == 0) {
			glDeleteShader(vs);
			glDeleteShader(fs);
			return 0;
		}
		glAttachShader(prog, vs);
		glAttachShader(prog, fs);
		glLinkProgram(prog);
		glDeleteShader(vs);
		glDeleteShader(fs);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
		if (linkStatus == GL_FALSE) {
			std::cerr << "Failed to link GLSL shader program: "
				<< vertexFile.string() << ", " << fragmentFile.string() << '\n';
			const std::string log = GetProgramInfoLog(prog);
			if (!log.empty())
				std::cerr << log << '\n';
			glDeleteProgram(prog);
			return 0;
		}
		return prog;
	}
}
