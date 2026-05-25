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
        positionLocation = glGetAttribLocation(program, "inPosition");
        normalLocation = glGetAttribLocation(program, "inNormal");
        uvLocation = glGetAttribLocation(program, "inUv");
        return true;
    }

    bool GlfwEdgeShader::Setup(const ViewerInfo& viewerInfo) {
        program = GlfwShaderCompiler::CreateShader(viewerInfo.shaderDir / "edge.vert", viewerInfo.shaderDir / "edge.frag");
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "inPosition");
        normalLocation = glGetAttribLocation(program, "inNormal");
        return true;
    }

    bool GlfwGroundShadowShader::Setup(const ViewerInfo& viewerInfo) {
        program = GlfwShaderCompiler::CreateShader(viewerInfo.shaderDir / "ground_shadow.vert", viewerInfo.shaderDir / "ground_shadow.frag");
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "inPosition");
        return true;
    }
}
