#include "GlfwViewer.h"

#include "GlfwInstance.h"

#include <iostream>

namespace Chrivent {
	const char* GlfwViewer::GetGpuTypeName(const std::string& renderer) {
		if (renderer.contains("NVIDIA") || renderer.contains("Radeon RX") || renderer.contains("Radeon Pro"))
			return "discrete";
		if (renderer.contains("Intel") || renderer.contains("Radeon(TM) Graphics"))
			return "integrated";
		return "other";
	}

	GlfwViewer::GlfwViewer() {
		info = std::make_unique<GlfwViewerInfo>();
	}

	void GlfwViewer::ConfigureGlfwHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
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
		const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		std::cout << "opengl_gpu=" << (renderer ? renderer : "unknown") << '\n';
		std::cout << "opengl_gpu_type=" << GetGpuTypeName(renderer ? renderer : "") << '\n';
		glfwSwapInterval(0);
		glEnable(GL_MULTISAMPLE);
		InitDirs("shader_glsl");
		info.shader = std::make_unique<GlfwModelShader>();
		if (!info.shader->Setup(info)) {
			std::cerr << "Failed to set up main GLFW shader.\n";
			return false;
		}
		info.edgeShader = std::make_unique<GlfwEdgeShader>();
		if (!info.edgeShader->Setup(info)) {
			std::cerr << "Failed to set up edge GLFW shader.\n";
			return false;
		}
		info.gsShader = std::make_unique<GlfwGroundShadowShader>();
		if (!info.gsShader->Setup(info)) {
			std::cerr << "Failed to set up ground shadow GLFW shader.\n";
			return false;
		}
		info.dummyColorTex = textureCache.CreateWhiteTexture().texture;
		if (info.dummyColorTex == 0)
			return false;
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
