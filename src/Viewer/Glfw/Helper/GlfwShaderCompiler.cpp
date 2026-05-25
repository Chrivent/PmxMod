#include "GlfwShaderCompiler.h"

#include "../../GlslPreprocessor.h"

#include <iostream>

namespace Chrivent {
	std::string GlfwShaderCompiler::BuildPreamble() {
		return R"(#version 460 core
#define PMX_MODEL_VERTEX_CONSTANTS layout(std140, binding = 0) uniform ModelVertexConstants { mat4 wv; mat4 wvp; } vertexConstants;
#define PMX_MODEL_PIXEL_CONSTANTS layout(std140, binding = 1) uniform ModelPixelConstants { vec4 diffuseAlpha; vec4 ambientSpecularPower; vec4 specular; vec4 lightColor; vec4 lightDir; vec4 texMulFactor; vec4 texAddFactor; vec4 toonTexMulFactor; vec4 toonTexAddFactor; vec4 sphereTexMulFactor; vec4 sphereTexAddFactor; ivec4 textureModes; } pixelConstants;
#define PMX_MODEL_TEX layout(binding = 0) uniform sampler2D tex;
#define PMX_MODEL_TOON_TEX layout(binding = 1) uniform sampler2D toonTex;
#define PMX_MODEL_SPHERE_TEX layout(binding = 2) uniform sampler2D sphereTex;
#define PMX_EDGE_VERTEX_CONSTANTS layout(std140, binding = 0) uniform EdgeVertexConstants { mat4 wv; mat4 wvp; vec2 screenSize; float edgeSize; } edgeConstants;
#define PMX_EDGE_PIXEL_CONSTANTS layout(std140, binding = 1) uniform EdgePixelConstants { vec4 edgeColor; } edgeConstants;
#define PMX_GROUND_SHADOW_VERTEX_CONSTANTS layout(std140, binding = 0) uniform GroundShadowVertexConstants { mat4 wvp; } shadowConstants;
#define PMX_GROUND_SHADOW_PIXEL_CONSTANTS layout(std140, binding = 1) uniform GroundShadowPixelConstants { vec4 shadowColor; } shadowConstants;
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
