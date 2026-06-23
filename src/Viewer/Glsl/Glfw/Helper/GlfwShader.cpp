#include "GlfwShader.h"

#include "GlfwShaderCompiler.h"

namespace Chrivent {
    GlfwShader::~GlfwShader() {
        if (program != 0)
            glDeleteProgram(program);
        program = 0;
    }

    bool GlfwModelShader::Setup(const std::filesystem::path& vertexShader, const std::filesystem::path& fragmentShader) {
        program = GlfwShaderCompiler::CreateShader(vertexShader, fragmentShader);
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "inPosition");
        normalLocation = glGetAttribLocation(program, "inNormal");
        uvLocation = glGetAttribLocation(program, "inUv");
        return true;
    }

    bool GlfwEdgeShader::Setup(const std::filesystem::path& vertexShader, const std::filesystem::path& fragmentShader) {
        program = GlfwShaderCompiler::CreateShader(vertexShader, fragmentShader);
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "inPosition");
        normalLocation = glGetAttribLocation(program, "inNormal");
        return true;
    }

    bool GlfwGroundShadowShader::Setup(const std::filesystem::path& vertexShader, const std::filesystem::path& fragmentShader) {
        program = GlfwShaderCompiler::CreateShader(vertexShader, fragmentShader);
        if (program == 0)
            return false;
        positionLocation = glGetAttribLocation(program, "inPosition");
        return true;
    }
}
