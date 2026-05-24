#pragma once

#include <filesystem>
#include <glad/glad.h>
#include <string>

namespace Chrivent {
	class GlfwShaderCompiler {
		// OpenGL ?곗씠????낆쓣 濡쒓렇??異쒕젰??臾몄옄?대줈 蹂?섑븳??
		static const char* ShaderTypeName(GLenum shaderType);
		// GLSL ?곗씠??而댄뙆???ㅽ뙣 ??OpenGL info log瑜?媛?몄삩??
		static std::string GetShaderInfoLog(GLuint shader);
		// GLSL ?꾨줈洹몃옩 留곹겕 ?ㅽ뙣 ??OpenGL info log瑜?媛?몄삩??
		static std::string GetProgramInfoLog(GLuint program);
		// GLSL ?뚯씪??臾몄옄?대줈 ?쎈뒗??
		static bool ReadShaderFile(const std::filesystem::path& file, std::string& code);

	public:
		// GLSL ?곗씠???뚯뒪瑜?吏?뺥븳 ??낆쑝濡?而댄뙆?쇳븳??
		static GLuint CompileShader(GLenum shaderType, const std::string& code);
		// 踰꾪뀓???꾨옒洹몃㉫??GLSL ?뚯씪??而댄뙆?쇳븯怨??꾨줈洹몃옩?쇰줈 留곹겕?쒕떎.
		static GLuint CreateShader(const std::filesystem::path& vertexFile, const std::filesystem::path& fragmentFile);
	};
}
