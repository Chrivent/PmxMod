#include "Viewer/Shader/OpenGlShader.h"

#include "Viewer/Shader/OpenGlShaderCompiler.h"

namespace Chrivent {
    OpenGlShader::~OpenGlShader() {
        if (program != 0)
            glDeleteProgram(program);
        program = 0;
    }

    bool OpenGlModelShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        normalLocation = 1;
        uvLocation = 2;
        return true;
    }

    bool OpenGlEdgeShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        normalLocation = 1;
        return true;
    }

    bool OpenGlGroundShadowShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        return true;
    }

    bool OpenGlDepthOnlyShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderCompiler::CreateVertexOnlyShader(pass.shaderPath, pass.vertexEntry);
        if (program == 0)
            return false;
        positionLocation = 0;
        normalLocation = 1;
        uvLocation = 2;
        return true;
    }

	bool OpenGlSceneVelocityShader::Initialize(const EffectPassDefinition& pass) {
		program = OpenGlShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry);
		if (program == 0)
			return false;
		positionLocation = 0;
		previousPositionLocation = 1;
		return true;
	}

    bool OpenGlPostProcessShader::Initialize(const EffectPassDefinition& pass) {
        program = OpenGlShaderCompiler::CreateShader(pass.shaderPath, pass.vertexEntry, pass.pixelEntry, true);
        return program != 0;
    }
}
