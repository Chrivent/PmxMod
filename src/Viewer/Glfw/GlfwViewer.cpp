#include "Viewer/Glfw/GlfwViewer.h"

#include "Viewer/Glfw/GlfwInstance.h"
#include "Viewer/Shader/ShaderPackage.h"

#include <algorithm>
#include <iostream>

namespace Chrivent {
	bool GlfwViewer::CreatePostProcessTargets() {
		ResetPostProcessTargets();
		if (screenWidth <= 0 || screenHeight <= 0)
			return false;
		postProcessSampleCount = std::max<GLsizei>(
			1, std::min<GLsizei>(msaaSamples, static_cast<GLsizei>(capabilities.maxSampleCount)));
		glCreateFramebuffers(1, &sceneFramebuffer);
		glCreateRenderbuffers(1, &sceneColorMsaa);
		glCreateRenderbuffers(1, &sceneDepthStencil);
		if (postProcessSampleCount > 1) {
			glNamedRenderbufferStorageMultisample(sceneColorMsaa, postProcessSampleCount, GL_RGBA8, screenWidth, screenHeight);
			glNamedRenderbufferStorageMultisample(sceneDepthStencil, postProcessSampleCount, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
		} else {
			glNamedRenderbufferStorage(sceneColorMsaa, GL_RGBA8, screenWidth, screenHeight);
			glNamedRenderbufferStorage(sceneDepthStencil, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
		}
		glNamedFramebufferRenderbuffer(sceneFramebuffer, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sceneColorMsaa);
		glNamedFramebufferRenderbuffer(sceneFramebuffer, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepthStencil);
		glCreateFramebuffers(1, &resolveFramebuffer);
		glCreateTextures(GL_TEXTURE_2D, 1, &sceneColorTexture);
		glTextureStorage2D(sceneColorTexture, 1, GL_RGBA8, screenWidth, screenHeight);
		glTextureParameteri(sceneColorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(sceneColorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(sceneColorTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(sceneColorTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glNamedFramebufferTexture(resolveFramebuffer, GL_COLOR_ATTACHMENT0, sceneColorTexture, 0);
		glCreateFramebuffers(2, pingPongFramebuffers);
		glCreateTextures(GL_TEXTURE_2D, 2, pingPongTextures);
		for (int index = 0; index < 2; index++) {
			glTextureStorage2D(pingPongTextures[index], 1, GL_RGBA8, screenWidth, screenHeight);
			glTextureParameteri(pingPongTextures[index], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(pingPongTextures[index], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(pingPongTextures[index], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(pingPongTextures[index], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glNamedFramebufferTexture(pingPongFramebuffers[index], GL_COLOR_ATTACHMENT0, pingPongTextures[index], 0);
			if (glCheckNamedFramebufferStatus(pingPongFramebuffers[index], GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				return false;
		}
		return glCheckNamedFramebufferStatus(sceneFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
			&& glCheckNamedFramebufferStatus(resolveFramebuffer, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	}

	void GlfwViewer::ResetPostProcessTargets() {
		glDeleteTextures(2, pingPongTextures);
		glDeleteFramebuffers(2, pingPongFramebuffers);
		if (sceneDepthStencil != 0)
			glDeleteRenderbuffers(1, &sceneDepthStencil);
		if (sceneColorMsaa != 0)
			glDeleteRenderbuffers(1, &sceneColorMsaa);
		if (sceneColorTexture != 0)
			glDeleteTextures(1, &sceneColorTexture);
		if (resolveFramebuffer != 0)
			glDeleteFramebuffers(1, &resolveFramebuffer);
		if (sceneFramebuffer != 0)
			glDeleteFramebuffers(1, &sceneFramebuffer);
		sceneDepthStencil = 0;
		sceneColorMsaa = 0;
		sceneColorTexture = 0;
		resolveFramebuffer = 0;
		sceneFramebuffer = 0;
		pingPongTextures[0] = 0;
		pingPongTextures[1] = 0;
		pingPongFramebuffers[0] = 0;
		pingPongFramebuffers[1] = 0;
		postProcessSampleCount = 1;
	}

	GlfwViewer::~GlfwViewer() {
		if (window)
			glfwMakeContextCurrent(window);
		ResetPostProcessTargets();
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
		const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		const auto* shaderVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
		GLint majorVersion = 0;
		GLint minorVersion = 0;
		GLint maxSamples = 1;
		GLint uniformAlignment = 1;
		GLint maxTextureBindings = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
		glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
		glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uniformAlignment);
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureBindings);
		if (majorVersion < 4 || (majorVersion == 4 && minorVersion < 6)) {
			std::cerr << "OpenGL 4.6 or newer is required.\n";
			return false;
		}
		capabilities.apiName = "OpenGL";
		capabilities.apiVersion = version ? version : "4.6";
		capabilities.shaderVersion = shaderVersion ? shaderVersion : "GLSL 4.60";
		capabilities.gpuName = renderer ? renderer : "unknown";
		capabilities.gpuType = ResolveGpuTypeName(capabilities.gpuName);
		capabilities.maxSampleCount = static_cast<uint32_t>(std::max(maxSamples, 1));
		capabilities.uniformBufferAlignment = static_cast<uint64_t>(std::max(uniformAlignment, 1));
		capabilities.maxTextureBindings = static_cast<uint32_t>(std::max(maxTextureBindings, 0));
		capabilities.shaderModelMajor = 4;
		capabilities.shaderModelMinor = 6;
		capabilities.Print();
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
		glCreateVertexArrays(1, &postProcessVao);
		return CreatePostProcessTargets();
	}

	bool GlfwViewer::Resize() {
		glViewport(0, 0, screenWidth, screenHeight);
		return CreatePostProcessTargets();
	}

	void GlfwViewer::BeginFrame() {
		glBindFramebuffer(GL_FRAMEBUFFER, postProcessShaders.empty() ? 0 : sceneFramebuffer);
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	bool GlfwViewer::EndFrame() {
		if (!postProcessShaders.empty()) {
			glBlitNamedFramebuffer(sceneFramebuffer, resolveFramebuffer,
				0, 0, screenWidth, screenHeight,
				0, 0, screenWidth, screenHeight,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_STENCIL_TEST);
			glDisable(GL_BLEND);
			glDisable(GL_CULL_FACE);
			glBindVertexArray(postProcessVao);
			GLuint sourceTexture = sceneColorTexture;
			for (size_t index = 0; index < postProcessShaders.size(); index++) {
				const bool lastPass = index + 1 == postProcessShaders.size();
				const size_t targetIndex = index % 2;
				glBindFramebuffer(GL_FRAMEBUFFER, lastPass ? 0 : pingPongFramebuffers[targetIndex]);
				glViewport(0, 0, screenWidth, screenHeight);
				glUseProgram(postProcessShaders[index]->program);
				glBindTextureUnit(0, sourceTexture);
				glDrawArrays(GL_TRIANGLES, 0, 3);
				sourceTexture = pingPongTextures[targetIndex];
			}
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
		postProcessShaders.clear();
		postProcessShaders.push_back(std::move(shader));
		return true;
	}

	bool GlfwViewer::LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) {
		ClearPostProcessEffect();
		for (const auto* effect : effects) {
			if (!effect || effect->passes.empty())
				continue;
			auto shader = std::make_unique<GlfwPostProcessShader>();
			if (!shader->Initialize(effect->passes.front())) {
				ClearPostProcessEffect();
				return false;
			}
			postProcessShaders.push_back(std::move(shader));
		}
		return true;
	}

	void GlfwViewer::ClearPostProcessEffect() {
		postProcessShaders.clear();
	}

	std::unique_ptr<Instance> GlfwViewer::CreateInstance() const {
		return std::make_unique<GlfwInstance>();
	}

}
