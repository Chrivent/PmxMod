#include "Viewer/Glfw/Helper/GlfwShader.h"

#include "Viewer/Glfw/Helper/GlfwShaderCompiler.h"

namespace Chrivent {
    GlfwShader::~GlfwShader() {
        if (program != 0)
            glDeleteProgram(program);
        program = 0;
    }

    bool GlfwModelShader::Setup(const EffectPassDefinition& pass) {
        program = GlfwShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        normalLocation = 1;
        uvLocation = 2;
        return true;
    }

    bool GlfwEdgeShader::Setup(const EffectPassDefinition& pass) {
        program = GlfwShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        normalLocation = 1;
        return true;
    }

    bool GlfwGroundShadowShader::Setup(const EffectPassDefinition& pass) {
        program = GlfwShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        return true;
    }

    bool GlfwPostProcessShader::Setup(const EffectPassDefinition& pass) {
        program = GlfwShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry, true);
        return program != 0;
    }
}
