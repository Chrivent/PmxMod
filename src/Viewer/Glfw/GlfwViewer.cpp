#include "Viewer/Glfw/GlfwViewer.h"

#include "Viewer/Glfw/GlfwInstance.h"
#include "Viewer/Shader/ShaderPackage.h"

#include <algorithm>
#include <iostream>

namespace Chrivent {
	GlfwViewer::~GlfwViewer() {
		if (window)
			glfwMakeContextCurrent(window);
		postProcess.Reset();
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
		depthOnlyShader = std::make_unique<GlfwDepthOnlyShader>();
		if (!depthOnlyShader->Initialize(modelPass)) {
			std::cerr << "Failed to set up depth-only GLFW shader.\n";
			return false;
		}
		EffectPassDefinition sceneVelocityPass{
			.shaderPath = resourceDir / "shaders" / "pmxmod-default" / "internal" / "scene-velocity.hlsl"
		};
		sceneVelocityShader = std::make_unique<GlfwSceneVelocityShader>();
		if (!sceneVelocityShader->Initialize(sceneVelocityPass)) {
			std::cerr << "Failed to set up scene velocity GLFW shader.\n";
			return false;
		}
		dummyColorTex = textureCache.CreateWhiteTexture().texture;
		if (dummyColorTex == 0)
			return false;
		return postProcess.InitializeTargets(screenWidth, screenHeight, msaaSamples, capabilities.maxSampleCount);
	}

	bool GlfwViewer::Resize() {
		glViewport(0, 0, screenWidth, screenHeight);
		return postProcess.InitializeTargets(screenWidth, screenHeight, msaaSamples, capabilities.maxSampleCount);
	}

	void GlfwViewer::BeginFrame() {
		glBindFramebuffer(GL_FRAMEBUFFER, postProcess.ResolveSceneFramebuffer());
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

	bool GlfwViewer::EndFrame() {
		postProcess.Draw(screenWidth, screenHeight, postProcessFrameData);
		glfwSwapBuffers(window);
		return true;
	}

	bool GlfwViewer::BeginPostProcessDepthPass() {
		return postProcess.BeginDepthPass(screenWidth, screenHeight);
	}

	void GlfwViewer::EndPostProcessDepthPass() {
		postProcess.EndDepthPass();
	}

	void GlfwViewer::WaitIdle() {
		glfwMakeContextCurrent(window);
		glFinish();
	}

	bool GlfwViewer::LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) {
		const bool loaded = postProcess.Load(effects);
		if (loaded)
			ResetPostProcessFrameHistory();
		return loaded;
	}

	void GlfwViewer::ResetPostProcessHistory() {
		postProcess.ResetHistory();
		ResetPostProcessFrameHistory();
	}

	std::unique_ptr<Instance> GlfwViewer::CreateInstance() const {
		return std::make_unique<GlfwInstance>();
	}

}
