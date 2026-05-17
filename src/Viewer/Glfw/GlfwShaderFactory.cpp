#include "GlfwShaderFactory.h"

#include <fstream>

namespace Chrivent {
	GLuint GlfwShaderFactory::CompileShader(const GLenum shaderType, const std::string& code) {
		const GLuint shader = glCreateShader(shaderType);
		if (!shader)
			return 0;
		const char* codes = code.c_str();
		const auto codesLen = static_cast<GLint>(code.size());
		glShaderSource(shader, 1, &codes, &codesLen);
		glCompileShader(shader);
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
		if (!f)
			return 0;
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
		return prog;
	}
}
