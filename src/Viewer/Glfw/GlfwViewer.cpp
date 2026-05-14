#include "GlfwViewer.h"

#include "GlfwInstance.h"
#include "GlfwShaderHelper.h"

#include <iostream>
#include <ranges>
#include <stb_image.h>

namespace Chrivent {
	GlfwShader::~GlfwShader() {
		if (program != 0)
			glDeleteProgram(program);
		program = 0;
	}

	bool GlfwShader::Setup(const GlfwViewer& viewer) {
		program = GlfwShaderHelper::CreateShader(viewer.shaderDir / "mmd.glsl");
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
			"cartoonTexMode", "cartoonTex", "cartoonTexMulFactor", "cartoonTexAddFactor",
			"lightColor", "lightDir"
		};
		GLint* outs[] = {
			&wvLocation, &wvpLocation,
			&ambientLocation, &diffuseLocation, &specularLocation, &specularPowerLocation, &alphaLocation,
			&texModeLocation, &texLocation, &texMulFactorLocation, &texAddFactorLocation,
			&sphereTexModeLocation, &sphereTexLocation, &sphereTexMulFactorLocation, &sphereTexAddFactorLocation,
			&cartoonTexModeLocation, &cartoonTexLocation, &cartoonTexMulFactorLocation, &cartoonTexAddFactorLocation,
			&lightColorLocation, &lightDirLocation
		};
		for (int i = 0; i < std::size(names); i++)
			*outs[i] = glGetUniformLocation(program, names[i]);
		glUseProgram(program);
		glUniform1i(texLocation, 0);
		glUniform1i(sphereTexLocation, 1);
		glUniform1i(cartoonTexLocation, 2);
		return true;
	}

	GlfwEdgeShader::~GlfwEdgeShader() {
		if (program != 0)
			glDeleteProgram(program);
		program = 0;
	}

	bool GlfwEdgeShader::Setup(const GlfwViewer& viewer) {
		program = GlfwShaderHelper::CreateShader(viewer.shaderDir / "mmd_edge.glsl");
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

	bool GlfwGroundShadowShader::Setup(const GlfwViewer& viewer) {
		program = GlfwShaderHelper::CreateShader(viewer.shaderDir / "mmd_ground_shadow.glsl");
		if (program == 0)
			return false;
		positionLocation = glGetAttribLocation(program, "position");
		wvpLocation = glGetUniformLocation(program, "wvp");
		shadowColorLocation = glGetUniformLocation(program, "shadowColor");
		return true;
	}

	GlfwViewer::~GlfwViewer() {
		for (auto& [texture, hasAlpha] : textures | std::views::values)
			glDeleteTextures(1, &texture);
		if (dummyColorTex != 0)
			glDeleteTextures(1, &dummyColorTex);
	}

	void GlfwViewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, msaaSamples);
	}

	bool GlfwViewer::Setup() {
		glfwMakeContextCurrent(window);
		if (!gladLoadGLLoader(LoadGlProc)) {
			std::cout << "Failed to load OpenGL functions.\n";
			return false;
		}
		glfwSwapInterval(0);
		glEnable(GL_MULTISAMPLE);
		InitDirs("shader_Glfw");
		shader = std::make_unique<GlfwShader>();
		if (!shader->Setup(*this)) {
			std::cout << "Failed to set up main GLFW shader: " << (shaderDir / "mmd.glsl") << '\n';
			return false;
		}
		edgeShader = std::make_unique<GlfwEdgeShader>();
		if (!edgeShader->Setup(*this)) {
			std::cout << "Failed to set up edge GLFW shader: " << (shaderDir / "mmd_edge.glsl") << '\n';
			return false;
		}
		gsShader = std::make_unique<GlfwGroundShadowShader>();
		if (!gsShader->Setup(*this)) {
			std::cout << "Failed to set up ground shadow GLFW shader: " << (shaderDir / "mmd_ground_shadow.glsl") << '\n';
			return false;
		}
		glGenTextures(1, &dummyColorTex);
		glBindTexture(GL_TEXTURE_2D, dummyColorTex);
		glTexImage2D(GL_TEXTURE_2D, 0,
			GL_RGBA, 1, 1, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glBindTexture(GL_TEXTURE_2D, 0);
		return true;
	}

	bool GlfwViewer::Resize() {
		glViewport(0, 0, screenWidth, screenHeight);
		return true;
	}

	void GlfwViewer::BeginFrame() {
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	bool GlfwViewer::EndFrame() {
		glfwSwapBuffers(window);
		return true;
	}

	std::unique_ptr<Instance> GlfwViewer::CreateInstance() const {
		return std::make_unique<GlfwInstance>();
	}

	GlfwTexture GlfwViewer::LoadTexture(const std::filesystem::path& texturePath, const bool clamp) {
		const auto it = textures.find(texturePath);
		if (it != textures.end())
			return it->second;
		int x = 0, y = 0, comp = 0;
		stbi_uc* image = LoadImageRgba(texturePath, x, y, comp, true);
		if (!image)
			return GlfwTexture{ 0, false };
		const bool hasAlpha = comp == 4;
		GLuint tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0,
			GL_RGBA, x, y, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, image);
		stbi_image_free(image);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		if (clamp) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		textures[texturePath] = GlfwTexture{ tex, hasAlpha };
		return textures[texturePath];
	}
}
