#include "Viewer/Glfw/GlfwViewer.h"

#include "Viewer/Glfw/GlfwInstance.h"
#include "Viewer/Shader/ShaderPackage.h"

#include <algorithm>
#include <iostream>

namespace Chrivent {
	bool GlfwViewer::CreatePostProcessTargets() {
		DestroyPostProcessTargets();
		if (screenWidth <= 0 || screenHeight <= 0)
			return false;
		glGenFramebuffers(1, &sceneFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
		glGenTextures(1, &sceneColorTexture);
		glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, screenWidth, screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);
		glGenRenderbuffers(1, &sceneDepthStencil);
		glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthStencil);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthStencil);
		const bool succeeded = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return succeeded;
	}

	void GlfwViewer::DestroyPostProcessTargets() {
		if (sceneDepthStencil != 0)
			glDeleteRenderbuffers(1, &sceneDepthStencil);
		if (sceneColorTexture != 0)
			glDeleteTextures(1, &sceneColorTexture);
		if (sceneFramebuffer != 0)
			glDeleteFramebuffers(1, &sceneFramebuffer);
		sceneDepthStencil = 0;
		sceneColorTexture = 0;
		sceneFramebuffer = 0;
	}

	GlfwViewer::~GlfwViewer() {
		if (window)
			glfwMakeContextCurrent(window);
		DestroyPostProcessTargets();
		if (postProcessVao != 0)
			glDeleteVertexArrays(1, &postProcessVao);
	}

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
		InitDirs("shaders");
		ShaderPackage package;
		std::string error;
		if (!ShaderPackageParser::Load(resourceDir / "shaders" / "pmxmod-default" / "package.json", package, error)) {
			std::cerr << error << '\n';
			return false;
		}
		const auto modelEffect = std::ranges::find(package.effects, EffectType::Model, &EffectDefinition::type);
		const auto edgeEffect = std::ranges::find(package.effects, EffectType::Edge, &EffectDefinition::type);
		const auto groundShadowEffect = std::ranges::find(package.effects, EffectType::GroundShadow, &EffectDefinition::type);
		if (modelEffect == package.effects.end() || modelEffect->passes.empty()
			|| edgeEffect == package.effects.end() || edgeEffect->passes.empty()
			|| groundShadowEffect == package.effects.end() || groundShadowEffect->passes.empty())
			return false;
		const auto& modelPass = modelEffect->passes.front();
		const auto& edgePass = edgeEffect->passes.front();
		const auto& groundShadowPass = groundShadowEffect->passes.front();
		shader = std::make_unique<GlfwModelShader>();
		if (!shader->Initialize(modelPass)) {
			std::cerr << "Failed to set up main GLFW shader.\n";
			return false;
		}
		edgeShader = std::make_unique<GlfwEdgeShader>();
		if (!edgeShader->Initialize(edgePass)) {
			std::cerr << "Failed to set up edge GLFW shader.\n";
			return false;
		}
		gsShader = std::make_unique<GlfwGroundShadowShader>();
		if (!gsShader->Initialize(groundShadowPass)) {
			std::cerr << "Failed to set up ground shadow GLFW shader.\n";
			return false;
		}
		dummyColorTex = textureCache.CreateWhiteTexture().texture;
		if (dummyColorTex == 0)
			return false;
		glGenVertexArrays(1, &postProcessVao);
		return CreatePostProcessTargets();
	}

	bool GlfwViewer::Resize() {
		glViewport(0, 0, screenWidth, screenHeight);
		return CreatePostProcessTargets();
	}

	void GlfwViewer::BeginFrame() {
		glBindFramebuffer(GL_FRAMEBUFFER, postProcessShader ? sceneFramebuffer : 0);
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	bool GlfwViewer::EndFrame() {
		if (postProcessShader) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_STENCIL_TEST);
			glDisable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glUseProgram(postProcessShader->program);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
			glBindVertexArray(postProcessVao);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
		glfwSwapBuffers(window);
		return true;
	}

	void GlfwViewer::WaitIdle() {
		glfwMakeContextCurrent(window);
		glFinish();
	}

	bool GlfwViewer::LoadPostProcessEffect(const EffectDefinition& effect) {
		if (effect.passes.empty())
			return false;
		auto shader = std::make_unique<GlfwPostProcessShader>();
		if (!shader->Initialize(effect.passes.front()))
			return false;
		postProcessShader = std::move(shader);
		return true;
	}

	std::unique_ptr<Instance> GlfwViewer::CreateInstance() const {
		return std::make_unique<GlfwInstance>();
	}

}
