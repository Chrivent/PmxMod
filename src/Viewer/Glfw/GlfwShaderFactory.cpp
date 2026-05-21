#include "GlfwShaderFactory.h"

#include <fstream>
#include <iostream>
#include <string>

namespace Chrivent {
	const char* GlfwShaderFactory::ShaderTypeName(const GLenum shaderType) {
		switch (shaderType) {
		case GL_VERTEX_SHADER:
			return "vertex";
		case GL_FRAGMENT_SHADER:
			return "fragment";
		default:
			return "unknown";
		}
	}

	std::string GlfwShaderFactory::GetShaderInfoLog(const GLuint shader) {
		GLint logLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength <= 1)
			return {};
		std::string log(logLength, '\0');
		glGetShaderInfoLog(shader, logLength, nullptr, log.data());
		return log;
	}

	std::string GlfwShaderFactory::GetProgramInfoLog(const GLuint program) {
		GLint logLength = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength <= 1)
			return {};
		std::string log(logLength, '\0');
		glGetProgramInfoLog(program, logLength, nullptr, log.data());
		return log;
	}

	GLuint GlfwShaderFactory::CompileShader(const GLenum shaderType, const std::string& code) {
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

	std::string GlfwShaderFactory::InjectDefine(const std::string& src, const char* defineLine) {
		if (src.starts_with("#version")) {
			const auto nl = src.find('\n');
			if (nl != std::string::npos) {
				std::string out;
				out.reserve(src.size() + 64);
				out.append(src, 0, nl + 1);
				out.append(defineLine);
				out.push_back('\n');
				out.append(src, nl + 1, std::string::npos);
				return out;
			}
		}
		return std::string(defineLine) + "\n" + src;
	}

	GLuint GlfwShaderFactory::CreateShader(const std::filesystem::path& file) {
		std::ifstream f(file);
		if (!f) {
			std::cerr << "Failed to open GLSL shader file: " << file.string() << '\n';
			return 0;
		}
		const std::string src((std::istreambuf_iterator(f)), {});
		const std::string vsCode = InjectDefine(src, "#define VERTEX");
		const std::string fsCode = InjectDefine(src, "#define FRAGMENT");
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
			std::cerr << "Failed to link GLSL shader program: " << file.string() << '\n';
			const std::string log = GetProgramInfoLog(prog);
			if (!log.empty())
				std::cerr << log << '\n';
			glDeleteProgram(prog);
			return 0;
		}
		return prog;
	}
}
