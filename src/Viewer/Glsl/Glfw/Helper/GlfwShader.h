#pragma once

#include "../../../../Shader/ShaderPackage.h"

#include <glad/glad.h>

namespace Chrivent {
    struct GlfwShader {
        GLuint  program = 0;
        GLint   positionLocation = -1;

        virtual ~GlfwShader();
    };

    struct GlfwModelShader : GlfwShader {
        GLint   normalLocation = -1;
        GLint   uvLocation = -1;

        // 모델 렌더링 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Setup(const EffectPassDefinition& pass);
    };

    struct GlfwEdgeShader : GlfwShader {
        GLint   normalLocation = -1;

        // 엣지 렌더링 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Setup(const EffectPassDefinition& pass);
    };

    struct GlfwGroundShadowShader : GlfwShader {
        // 지면 그림자 셰이더 프로그램을 컴파일하고 attribute 위치를 조회한다.
        bool Setup(const EffectPassDefinition& pass);
    };
}
