#include "GlfwViewer.h"

#include "GlfwInstance.h"
#include "Helper/GlfwShaderFactory.h"

#include <iostream>

namespace Chrivent {
	GlfwShader::~GlfwShader() {
		if (program != 0)
			glDeleteProgram(program);
		program = 0;
	}

	bool GlfwShader::Setup(const ViewerInfo& viewerInfo) {
		program = GlfwShaderFactory::CreateShader(viewerInfo.shaderDir / "mmd.glsl");
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

	GlfwEdgeShader::~GlfwEdgeShader() {
		if (program != 0)
			glDeleteProgram(program);
		program = 0;
	}

	bool GlfwEdgeShader::Setup(const ViewerInfo& viewerInfo) {
		program = GlfwShaderFactory::CreateShader(viewerInfo.shaderDir / "mmd_edge.glsl");
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

	GlfwGroundShadowShader::~GlfwGroundShadowShader() {
		if (program != 0)
			glDeleteProgram(program);
		program = 0;
	}

	bool GlfwGroundShadowShader::Setup(const ViewerInfo& viewerInfo) {
		program = GlfwShaderFactory::CreateShader(viewerInfo.shaderDir / "mmd_ground_shadow.glsl");
		if (program == 0)
			return false;
		positionLocation = glGetAttribLocation(program, "position");
		wvpLocation = glGetUniformLocation(program, "wvp");
		shadowColorLocation = glGetUniformLocation(program, "shadowColor");
		return true;
	}

	GlfwViewer::GlfwViewer() {
		info = std::make_unique<GlfwViewerInfo>();
	}

	GlfwViewer::~GlfwViewer() {
		const auto& info = GetGlfwInfo();
		if (info.dummyColorTex != 0)
			glDeleteTextures(1, &info.dummyColorTex);
	}

	void GlfwViewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, msaaSamples);
	}

	bool GlfwViewer::Setup() {
		auto& info = GetGlfwInfo();
		glfwMakeContextCurrent(info.window);
		if (!gladLoadGLLoader(LoadGlProc)) {
			std::cerr << "Failed to load OpenGL functions.\n";
			return false;
		}
		glfwSwapInterval(0);
		glEnable(GL_MULTISAMPLE);
		InitDirs("shader_Glfw");
		info.shader = std::make_unique<GlfwShader>();
		if (!info.shader->Setup(info)) {
			std::cerr << "Failed to set up main GLFW shader: " << (info.shaderDir / "mmd.glsl") << '\n';
			return false;
		}
		info.edgeShader = std::make_unique<GlfwEdgeShader>();
		if (!info.edgeShader->Setup(info)) {
			std::cerr << "Failed to set up edge GLFW shader: " << (info.shaderDir / "mmd_edge.glsl") << '\n';
			return false;
		}
		info.gsShader = std::make_unique<GlfwGroundShadowShader>();
		if (!info.gsShader->Setup(info)) {
			std::cerr << "Failed to set up ground shadow GLFW shader: " << (info.shaderDir / "mmd_ground_shadow.glsl") << '\n';
			return false;
		}
		glGenTextures(1, &info.dummyColorTex);
		glBindTexture(GL_TEXTURE_2D, info.dummyColorTex);
		glTexImage2D(GL_TEXTURE_2D, 0,
			GL_RGBA, 1, 1, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glBindTexture(GL_TEXTURE_2D, 0);
		return true;
	}

	bool GlfwViewer::Resize() {
		const auto& info = GetGlfwInfo();
		glViewport(0, 0, info.screenWidth, info.screenHeight);
		return true;
	}

	void GlfwViewer::BeginFrame() {
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	bool GlfwViewer::EndFrame() {
		glfwSwapBuffers(GetInfo().window);
		return true;
	}

	std::unique_ptr<Instance> GlfwViewer::CreateInstance() const {
		return std::make_unique<GlfwInstance>();
	}

	GlfwTexture GlfwViewer::LoadTexture(const std::filesystem::path& texturePath, const bool clamp) {
		return textureCache.Load(texturePath, clamp);
	}
}
