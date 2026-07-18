#pragma once

#include "Viewer/Shader/ShaderProgramDefinition.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <glad/glad.h>

namespace Chrivent {
    // HLSL을 SPIR-V와 GLSL로 변환해 OpenGL 프로그램을 생성한다.
    class OpenGlShaderBuilder {
        // SPIR-V를 GLSL로 변환하고 OpenGL 셰이더 객체로 만든다.
        static GLuint CreateStage(GLenum shaderType, const std::vector<uint32_t>& code,
			const std::string& entry, std::string& error);
        // OpenGL 셰이더 컴파일 로그를 읽는다.
        static std::string ReadShaderLog(GLuint shader);
        // OpenGL 프로그램 링크 로그를 읽는다.
        static std::string ReadProgramLog(GLuint program);

    public:
        // HLSL의 버텍스와 픽셀 진입점을 SPIR-V로 컴파일하고 프로그램으로 링크한다.
        static GLuint CreateShader(const std::filesystem::path& shaderFile, const std::string& vertexEntry,
			const std::string& pixelEntry, SpirvBindingProfile bindingProfile,
			std::string& error, bool invertVertexY = false);
    };
    
    // OpenGL 프로그램과 공통 position attribute 위치를 보관한다.
    struct OpenGlShader {
        GLuint  program = 0;
        GLint   positionLocation = -1;

		OpenGlShader() = default;
		~OpenGlShader();

		OpenGlShader(const OpenGlShader&) = delete;
		OpenGlShader& operator=(const OpenGlShader&) = delete;
    };

    // PMX 모델 표면 렌더링용 OpenGL 셰이더 프로그램을 나타낸다.
    struct OpenGlModelShader : OpenGlShader {
        GLint   normalLocation = -1;
        GLint   uvLocation = -1;

        // 모델 렌더링 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Initialize(const ShaderProgramDefinition& shaderProgram, std::string& error);
    };

    // PMX 외곽선 렌더링용 OpenGL 셰이더 프로그램을 나타낸다.
    struct OpenGlEdgeShader : OpenGlShader {
        GLint   normalLocation = -1;

        // 엣지 렌더링 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Initialize(const ShaderProgramDefinition& shaderProgram, std::string& error);
    };

    // PMX 지면 그림자 렌더링용 OpenGL 셰이더 프로그램을 나타낸다.
    struct OpenGlGroundShadowShader : OpenGlShader {
        // 지면 그림자 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Initialize(const ShaderProgramDefinition& shaderProgram, std::string& error);
    };

    // 공통 장면 depth 입력과 texture alpha cutout을 처리하는 OpenGL 프로그램을 나타낸다.
    struct OpenGlDepthOnlyShader : OpenGlShader {
        GLint uvLocation = -1;

		// 공통 장면 depth 프로그램을 컴파일한다.
		bool Initialize(const ShaderProgramDefinition& shaderProgram, std::string& error);
    };

	// 공통 장면 velocity 입력을 기록하는 OpenGL 프로그램을 나타낸다.
	struct OpenGlSceneVelocityShader : OpenGlShader {
		GLint previousPositionLocation = -1;
		GLint uvLocation = -1;

		// 공통 장면 velocity 프로그램을 컴파일하고 이전 위치와 UV attribute를 설정한다.
		bool Initialize(const ShaderProgramDefinition& shaderProgram, std::string& error);
	};

    // 풀스크린 후처리 패스를 실행하는 OpenGL 프로그램을 나타낸다.
    struct OpenGlPostProcessShader : OpenGlShader {
        // 후처리 HLSL을 OpenGL 프로그램으로 컴파일한다.
        bool Initialize(const ShaderProgramDefinition& shaderProgram, std::string& error);
    };
}
