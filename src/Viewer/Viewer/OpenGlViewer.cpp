#include "Viewer/Viewer/OpenGlViewer.h"

#include "Viewer/Instance/OpenGlInstance.h"
#include <algorithm>
#include <iostream>

// NVIDIA Optimus가 OpenGL 프로세스에 고성능 GPU를 우선 배정하도록 요청한다.
// ReSharper disable once CppInconsistentNaming
extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
// ReSharper disable once CommentTypo
// AMD PowerXpress가 OpenGL 프로세스에 고성능 GPU를 우선 배정하도록 요청한다.
// ReSharper disable once CppInconsistentNaming
// ReSharper disable once IdentifierTypo
extern "C" __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

namespace Chrivent {
	OpenGlViewer::~OpenGlViewer() {
		if (window)
			glfwMakeContextCurrent(window);
		postProcess.ResetResources();
	}

	const char* OpenGlViewer::ResolveGpuTypeName(const std::string& renderer) {
		if (renderer.contains("NVIDIA") || renderer.contains("Radeon RX") || renderer.contains("Radeon Pro"))
			return "discrete";
		if (renderer.contains("Intel") || renderer.contains("Radeon(TM) Graphics"))
			return "integrated";
		return "other";
	}

	void OpenGlViewer::ConfigureWindowHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, msaaSamples);
	}

	bool OpenGlViewer::SetupCore() {
		BindPostProcess(postProcess);
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
		capabilities.activeSampleCount = std::min<uint32_t>(msaaSamples, capabilities.maxSampleCount);
		capabilities.uniformBufferAlignment = static_cast<uint64_t>(std::max(uniformAlignment, 1));
		capabilities.maxTextureBindings = static_cast<uint32_t>(std::max(maxTextureBindings, 0));
		capabilities.shaderModelMajor = 4;
		capabilities.shaderModelMinor = 6;
		capabilities.Print();
		glfwSwapInterval(0);
		glEnable(GL_MULTISAMPLE);
		if (!pipeline.Initialize(builtInShaderPasses, sceneInputShaderPasses))
			return false;
		const GLuint dummyColorTexture = textureCache.CreateWhiteTexture().texture;
		if (dummyColorTexture == 0)
			return false;
		drawContext.SetDummyColorTexture(dummyColorTexture);
		glViewport(0, 0, screenWidth, screenHeight);
		return !postProcess.HasEffects()
			|| postProcess.InitializeTargets(screenWidth, screenHeight, capabilities.activeSampleCount);
	}

	bool OpenGlViewer::ResizeCore() {
		glViewport(0, 0, screenWidth, screenHeight);
		if (postProcess.HasEffects())
			return postProcess.InitializeTargets(
				screenWidth, screenHeight, capabilities.activeSampleCount);
		postProcess.ResetResources();
		return true;
	}

	FrameBeginResult OpenGlViewer::BeginFrameCore() {
		glBindFramebuffer(GL_FRAMEBUFFER, postProcess.GetSceneFramebuffer());
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		return FrameBeginResult::Ready;
	}

	FrameEndResult OpenGlViewer::EndFrameCore() {
		if (!postProcess.Draw(screenWidth, screenHeight, GetPostProcessFrameData()))
			return FrameEndResult::Failed;
		glfwSwapBuffers(window);
		return FrameEndResult::Presented;
	}

	bool OpenGlViewer::BeginPostProcessSceneInputPassCore() {
		return postProcess.BeginSceneInputPass(screenWidth, screenHeight);
	}

	bool OpenGlViewer::EndPostProcessSceneInputPassCore() {
		postProcess.EndSceneInputPass();
		return true;
	}

	bool OpenGlViewer::WaitIdle() {
		if (window == nullptr)
			return false;
		glfwMakeContextCurrent(window);
		glFinish();
		return true;
	}

	bool OpenGlViewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		return postProcess.Configure(
			screenWidth, screenHeight, capabilities.activeSampleCount, effects);
	}

	std::unique_ptr<Instance> OpenGlViewer::CreateInstanceCore() {
		return std::make_unique<OpenGlInstance>(*this, textureCache, drawContext);
	}

}
