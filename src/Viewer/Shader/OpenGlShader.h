#pragma once

#include "Viewer/Effect/ShaderPackage.h"

#include <glad/glad.h>

namespace Chrivent {
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
