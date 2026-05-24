#pragma once

#include <filesystem>
#include <glad/glad.h>

namespace Chrivent {
	class GlfwShaderCompiler {
		// OpenGL 셰이더 타입을 로그에 출력할 문자열로 변환한다.
		static const char* ShaderTypeName(GLenum shaderType);
		// GLSL 셰이더 컴파일 실패 시 OpenGL info log를 가져온다.
		static std::string GetShaderInfoLog(GLuint shader);
		// GLSL 프로그램 링크 실패 시 OpenGL info log를 가져온다.
		static std::string GetProgramInfoLog(GLuint program);
		// GLSL 파일을 읽어 전처리된 최종 소스를 만든다.
		static bool ReadShaderFile(const std::filesystem::path& file, std::string& code);

	public:
		// GLSL 셰이더 소스를 지정한 타입으로 컴파일한다.
		static GLuint CompileShader(GLenum shaderType, const std::string& code);
		// 버텍스/프래그먼트 GLSL 파일을 컴파일하고 프로그램으로 링크한다.
		static GLuint CreateShader(const std::filesystem::path& vertexFile, const std::filesystem::path& fragmentFile);
	};
}
