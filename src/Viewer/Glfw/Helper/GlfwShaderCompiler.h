#pragma once

#include <filesystem>
#include <glad/glad.h>
#include <string>
#include <vector>

namespace Chrivent {
	class GlfwShaderCompiler {
		// SPIR-V를 GLSL로 변환하고 OpenGL 셰이더 객체로 만든다.
		static GLuint CreateStage(GLenum shaderType, const std::vector<uint32_t>& code, const std::string& entry);
		// OpenGL 셰이더 컴파일 로그를 읽는다.
		static std::string ReadShaderLog(GLuint shader);
		// OpenGL 프로그램 링크 로그를 읽는다.
		static std::string ReadProgramLog(GLuint program);

	public:
		// HLSL의 버텍스와 픽셀 진입점을 SPIR-V로 컴파일하고 프로그램으로 링크한다.
		static GLuint CreateShader(const std::filesystem::path& shaderFile, const std::string& vertexEntry,
			const std::string& pixelEntry, bool invertVertexY = false);
		// HLSL의 버텍스 진입점만 SPIR-V로 컴파일하고 depth-only 프로그램으로 링크한다.
		static GLuint CreateVertexOnlyShader(const std::filesystem::path& shaderFile, const std::string& vertexEntry);
	};
}
