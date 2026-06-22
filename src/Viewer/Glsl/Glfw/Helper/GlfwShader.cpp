#include "GlfwShader.h"

#include "GlfwShaderCompiler.h"

namespace Chrivent {
    GlfwShader::~GlfwShader() {
        if (program != 0)
            glDeleteProgram(program);
        program = 0;
    }

    bool GlfwModelShader::Setup(const Viewer& viewer) {
        program = GlfwShaderCompiler::CreateShader(viewer.shaderDir / "model.vert", viewer.shaderDir / "model.frag");
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "inPosition");
        normalLocation = glGetAttribLocation(program, "inNormal");
        uvLocation = glGetAttribLocation(program, "inUv");
        return true;
    }

    bool GlfwEdgeShader::Setup(const Viewer& viewer) {
        program = GlfwShaderCompiler::CreateShader(viewer.shaderDir / "edge.vert", viewer.shaderDir / "edge.frag");
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "inPosition");
        normalLocation = glGetAttribLocation(program, "inNormal");
        return true;
    }

    bool GlfwGroundShadowShader::Setup(const Viewer& viewer) {
        program = GlfwShaderCompiler::CreateShader(viewer.shaderDir / "ground_shadow.vert", viewer.shaderDir / "ground_shadow.frag");
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "inPosition");
        return true;
    }
}
