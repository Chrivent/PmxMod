#pragma once

#include "../../Viewer.h"

#include <glad/glad.h>

namespace Chrivent {
    struct GlfwShader {
        GLuint  program = 0;
        GLint   positionLocation = -1;
        GLint   wvpLocation = -1;

        virtual ~GlfwShader();
    };

    struct GlfwModelShader : GlfwShader {
        GLint   normalLocation = -1;
        GLint   uvLocation = -1;
        GLint   wvLocation = -1;
        GLint   ambientLocation = -1;
        GLint   diffuseLocation = -1;
        GLint   specularLocation = -1;
        GLint   specularPowerLocation = -1;
        GLint   alphaLocation = -1;
        GLint   texModeLocation = -1;
        GLint   texLocation = -1;
        GLint   texMulFactorLocation = -1;
        GLint   texAddFactorLocation = -1;
        GLint   sphereTexModeLocation = -1;
        GLint   sphereTexLocation = -1;
        GLint   sphereTexMulFactorLocation = -1;
        GLint   sphereTexAddFactorLocation = -1;
        GLint   toonTexModeLocation = -1;
        GLint   toonTexLocation = -1;
        GLint   toonTexMulFactorLocation = -1;
        GLint   toonTexAddFactorLocation = -1;
        GLint   lightColorLocation = -1;
        GLint   lightDirLocation = -1;

        // 모델 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
        bool Setup(const ViewerInfo& viewerInfo);
    };

    struct GlfwEdgeShader : GlfwShader {
        GLint   normalLocation = -1;
        GLint   wvLocation = -1;
        GLint   screenSizeLocation = -1;
        GLint   edgeSizeLocation = -1;
        GLint   edgeColorLocation = -1;

        // 엣지 렌더링 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
        bool Setup(const ViewerInfo& viewerInfo);
    };

    struct GlfwGroundShadowShader : GlfwShader {
        GLint   shadowColorLocation = -1;

        // 지면 그림자 셰이더 프로그램을 컴파일하고 uniform 위치를 조회한다.
        bool Setup(const ViewerInfo& viewerInfo);
    };
}
