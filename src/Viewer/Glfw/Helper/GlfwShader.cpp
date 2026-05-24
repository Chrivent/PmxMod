#include "GlfwShader.h"

#include "GlfwShaderCompiler.h"

namespace Chrivent {
    GlfwShader::~GlfwShader() {
        if (program != 0)
            glDeleteProgram(program);
        program = 0;
    }

    bool GlfwModelShader::Setup(const ViewerInfo& viewerInfo) {
        program = GlfwShaderCompiler::CreateShader(viewerInfo.shaderDir / "model.vert", viewerInfo.shaderDir / "model.frag");
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "position");
        normalLocation = glGetAttribLocation(program, "normal");
        uvLocation  = glGetAttribLocation(program, "uv");
        const char* names[] = {
            "wv", "wvp",
            "ambient", "diffuse", "specular", "specularPower", "alpha",
            "texMode", "tex", "texMulFactor", "texAddFactor",
            "sphereTexMode", "sphereTex", "sphereTexMulFactor", "sphereTexAddFactor",
            "toonTexMode", "toonTex", "toonTexMulFactor", "toonTexAddFactor",
            "lightColor", "lightDir"
        };
        GLint* outs[] = {
            &wvLocation, &wvpLocation,
            &ambientLocation, &diffuseLocation, &specularLocation, &specularPowerLocation, &alphaLocation,
            &texModeLocation, &texLocation, &texMulFactorLocation, &texAddFactorLocation,
            &sphereTexModeLocation, &sphereTexLocation, &sphereTexMulFactorLocation, &sphereTexAddFactorLocation,
            &toonTexModeLocation, &toonTexLocation, &toonTexMulFactorLocation, &toonTexAddFactorLocation,
            &lightColorLocation, &lightDirLocation
        };
        for (int i = 0; i < std::size(names); i++)
            *outs[i] = glGetUniformLocation(program, names[i]);
        glUseProgram(program);
        glUniform1i(texLocation, 0);
        glUniform1i(sphereTexLocation, 1);
        glUniform1i(toonTexLocation, 2);
        return true;
    }

    bool GlfwEdgeShader::Setup(const ViewerInfo& viewerInfo) {
        program = GlfwShaderCompiler::CreateShader(viewerInfo.shaderDir / "edge.vert", viewerInfo.shaderDir / "edge.frag");
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "position");
        normalLocation = glGetAttribLocation(program, "normal");
        wvLocation = glGetUniformLocation(program, "wv");
        wvpLocation = glGetUniformLocation(program, "wvp");
        screenSizeLocation = glGetUniformLocation(program, "screenSize");
        edgeSizeLocation = glGetUniformLocation(program, "edgeSize");
        edgeColorLocation = glGetUniformLocation(program, "edgeColor");
        return true;
    }

    bool GlfwGroundShadowShader::Setup(const ViewerInfo& viewerInfo) {
        program = GlfwShaderCompiler::CreateShader(viewerInfo.shaderDir / "ground_shadow.vert", viewerInfo.shaderDir / "ground_shadow.frag");
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "position");
        wvpLocation = glGetUniformLocation(program, "wvp");
        shadowColorLocation = glGetUniformLocation(program, "shadowColor");
        return true;
    }
}
