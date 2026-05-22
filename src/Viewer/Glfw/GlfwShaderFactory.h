#pragma once

#include <filesystem>
#include <glad/glad.h>

namespace Chrivent {
	class GlfwShaderFactory {
		// OpenGL 셰이더 타입을 로그에 출력할 문자열로 변환한다.
		static const char* ShaderTypeName(GLenum shaderType);
		// GLSL 셰이더 컴파일 실패 시 OpenGL info log를 가져온다.
		static std::string GetShaderInfoLog(GLuint shader);
		// GLSL 프로그램 링크 실패 시 OpenGL info log를 가져온다.
		static std::string GetProgramInfoLog(GLuint program);

	public:
		// GLSL 셰이더 소스를 지정한 타입으로 컴파일한다.
		static GLuint CompileShader(GLenum shaderType, const std::string& code);
		// 단일 GLSL 파일에서 버텍스/프래그먼트 분기를 위한 define 줄을 삽입한다.
		static std::string InjectDefine(const std::string& src, const char* defineLine);
		// GLSL 파일을 읽어 버텍스/프래그먼트 셰이더를 생성한다.
		static GLuint CreateShader(const std::filesystem::path& file);
	};
}

