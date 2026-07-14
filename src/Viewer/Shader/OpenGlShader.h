#pragma once

#include "Viewer/Shader/ShaderPackage.h"

#include <glad/glad.h>

namespace Chrivent {
    class OpenGlShaderBuilder {
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
    
    struct OpenGlShader {
        GLuint  program = 0;
        GLint   positionLocation = -1;

        virtual ~OpenGlShader();
    };

    struct OpenGlModelShader : OpenGlShader {
        GLint   normalLocation = -1;
        GLint   uvLocation = -1;

        // 모델 렌더링 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Initialize(const EffectPassDefinition& pass);
    };

    struct OpenGlEdgeShader : OpenGlShader {
        GLint   normalLocation = -1;

        // 엣지 렌더링 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Initialize(const EffectPassDefinition& pass);
    };

    struct OpenGlGroundShadowShader : OpenGlShader {
        // 지면 그림자 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Initialize(const EffectPassDefinition& pass);
    };

    struct OpenGlDepthOnlyShader : OpenGlShader {
        GLint normalLocation = -1;
        GLint uvLocation = -1;

        // 포스트 프로세스 depth-only 패스용 버텍스 전용 프로그램을 컴파일한다.
        bool Initialize(const EffectPassDefinition& pass);
    };

	struct OpenGlSceneVelocityShader : OpenGlShader {
		GLint previousPositionLocation = -1;

		// 포스트 프로세스용 장면 속도 프로그램을 컴파일하고 이전 위치 attribute를 설정한다.
		bool Initialize(const EffectPassDefinition& pass);
	};

    struct OpenGlPostProcessShader : OpenGlShader {
        // 후처리 HLSL을 OpenGL 프로그램으로 컴파일한다.
        bool Initialize(const EffectPassDefinition& pass);
    };
}
