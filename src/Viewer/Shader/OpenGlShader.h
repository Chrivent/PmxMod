#pragma once

#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/ShaderProgramDefinition.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <glad/glad.h>

namespace Chrivent {
	// HLSL을 SPIR-V와 GLSL로 변환해 OpenGL 프로그램을 생성한다.
	class OpenGlProgramBuilder {
		// SPIR-V를 GLSL로 변환하고 OpenGL 셰이더 객체로 만든다.
		static GraphicsResult<GLuint> CreateStage(
			GLenum shaderType, const std::vector<uint32_t>& code, const std::string& entry);
		// OpenGL 셰이더 컴파일 로그를 읽는다.
		static std::string ReadShaderLog(GLuint shader);
		// OpenGL 프로그램 링크 로그를 읽는다.
		static std::string ReadProgramLog(GLuint program);

	public:
		// HLSL의 버텍스와 픽셀 진입점을 SPIR-V로 컴파일하고 프로그램으로 링크한다.
		static GraphicsResult<GLuint> CreateProgram(
			const std::filesystem::path& shaderFile, const std::string& vertexEntry,
			const std::string& pixelEntry, SpirvBindingProfile bindingProfile,
			bool invertVertexY = false);
	};

	// 생성된 OpenGL 프로그램의 수명을 관리한다.
	class OpenGlShaderProgram {
		GLuint program = 0;

		// 현재 소유한 OpenGL 프로그램을 해제한다.
		void Reset();

	protected:
		~OpenGlShaderProgram();

		// 지정한 바인딩 규격으로 OpenGL 프로그램을 생성하고 기존 프로그램과 교체한다.
		GraphicsResult<void> InitializeProgram(const ShaderProgramDefinition& shaderProgram,
			SpirvBindingProfile bindingProfile, bool invertVertexY = false);

	public:
		OpenGlShaderProgram() = default;

		OpenGlShaderProgram(const OpenGlShaderProgram&) = delete;
		OpenGlShaderProgram& operator=(const OpenGlShaderProgram&) = delete;
		OpenGlShaderProgram(OpenGlShaderProgram&& other) noexcept;
		OpenGlShaderProgram& operator=(OpenGlShaderProgram&& other) noexcept;

		GLuint GetProgram() const { return program; }
	};

	// 내장 장면 패스를 공통 바인딩 규격으로 생성하는 OpenGL 프로그램을 나타낸다.
	class OpenGlSceneShaderProgram final : public OpenGlShaderProgram {
	public:
		// 장면 렌더링용 HLSL 프로그램을 컴파일한다.
		GraphicsResult<void> Initialize(const ShaderProgramDefinition& shaderProgram);
	};

	// 풀스크린 후처리 패스를 실행하는 OpenGL 프로그램을 나타낸다.
	class OpenGlPostProcessShaderProgram final : public OpenGlShaderProgram {
	public:
		// 후처리 HLSL을 OpenGL 프로그램으로 컴파일한다.
		GraphicsResult<void> Initialize(const ShaderProgramDefinition& shaderProgram);
	};
}
