#include "GlfwViewer.h"

#include "GlfwInstance.h"

#include <iostream>

namespace Chrivent {
	const char* GlfwViewer::ResolveGpuTypeName(const std::string& renderer) {
		if (renderer.contains("NVIDIA") || renderer.contains("Radeon RX") || renderer.contains("Radeon Pro"))
			return "discrete";
		if (renderer.contains("Intel") || renderer.contains("Radeon(TM) Graphics"))
			return "integrated";
		return "other";
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
		glfwMakeContextCurrent(window);
		if (!gladLoadGLLoader(LoadGlProc)) {
			std::cerr << "Failed to load OpenGL functions.\n";
			return false;
		}
		const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		std::cout << "opengl_gpu=" << (renderer ? renderer : "unknown") << '\n';
		std::cout << "opengl_gpu_type=" << ResolveGpuTypeName(renderer ? renderer : "") << '\n';
		glfwSwapInterval(0);
		glEnable(GL_MULTISAMPLE);
		InitDirs("shader_glsl");
		shader = std::make_unique<GlfwModelShader>();
		if (!shader->Setup(*this)) {
			std::cerr << "Failed to set up main GLFW shader.\n";
			return false;
		}
		edgeShader = std::make_unique<GlfwEdgeShader>();
		if (!edgeShader->Setup(*this)) {
			std::cerr << "Failed to set up edge GLFW shader.\n";
			return false;
		}
		gsShader = std::make_unique<GlfwGroundShadowShader>();
		if (!gsShader->Setup(*this)) {
			std::cerr << "Failed to set up ground shadow GLFW shader.\n";
			return false;
		}
		dummyColorTex = textureCache.CreateWhiteTexture().texture;
		if (dummyColorTex == 0)
			return false;
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

	void GlfwViewer::WaitIdle() {
		glfwMakeContextCurrent(window);
		glFinish();
	}

	std::unique_ptr<Instance> GlfwViewer::CreateInstance() const {
		return std::make_unique<GlfwInstance>();
	}

}
