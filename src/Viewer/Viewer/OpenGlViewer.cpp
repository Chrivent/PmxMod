#include "Viewer/Viewer/OpenGlViewer.h"

#include "Viewer/Instance/OpenGlInstance.h"

#include <algorithm>

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

	GraphicsResult<void> OpenGlViewer::SetupCore(const SceneShaderRuntimeContract& shaderContract) {
		BindPostProcess(postProcess);
		glfwMakeContextCurrent(window);
		if (!gladLoadGLLoader(LoadGlProc))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InitializationFailed,
				"load OpenGL functions", "GLAD could not resolve the required functions"));
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
		if (majorVersion < 4 || (majorVersion == 4 && minorVersion < 6))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::UnsupportedFeature,
				"check OpenGL version", "OpenGL 4.6 or newer is required"));
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
		if (!pipeline.Initialize(shaderContract.builtIn, shaderContract.sceneInput))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize rendering pipeline", "the OpenGL pipeline could not be created"));
		const GLuint dummyColorTexture = textureCache.CreateWhiteTexture().texture;
		if (dummyColorTexture == 0)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"create dummy texture", "the fallback texture could not be created"));
		drawContext.SetDummyColorTexture(dummyColorTexture);
		glViewport(0, 0, screenWidth, screenHeight);
		if (postProcess.HasEffects()
			&& !postProcess.InitializeTargets(screenWidth, screenHeight, capabilities.activeSampleCount)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"initialize post-process targets", "the OpenGL post-process targets could not be created"));
		}
		return {};
	}

	GraphicsResult<void> OpenGlViewer::ResizeCore() {
		glViewport(0, 0, screenWidth, screenHeight);
		if (postProcess.HasEffects()) {
			if (!postProcess.InitializeTargets(screenWidth, screenHeight, capabilities.activeSampleCount))
				return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
					"resize post-process targets", "the OpenGL post-process targets could not be recreated"));
			return {};
		}
		postProcess.ResetResources();
		return {};
	}

	GraphicsResult<FrameBeginState> OpenGlViewer::BeginFrameCore() {
		glBindFramebuffer(GL_FRAMEBUFFER, postProcess.GetSceneFramebuffer());
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		return FrameBeginState::Ready;
	}

	GraphicsResult<FrameEndState> OpenGlViewer::EndFrameCore() {
		if (!postProcess.Draw(screenWidth, screenHeight, GetPostProcessFrameData()))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"draw post-process effects", "the OpenGL post-process chain failed"));
		glfwSwapBuffers(window);
		return FrameEndState::Presented;
	}

	GraphicsResult<void> OpenGlViewer::BeginPostProcessSceneInputPassCore() {
		if (!postProcess.BeginSceneInputPass(screenWidth, screenHeight))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::CommandRecordingFailed,
				"begin post-process scene input pass", "the OpenGL scene input pass could not begin"));
		return {};
	}

	GraphicsResult<void> OpenGlViewer::EndPostProcessSceneInputPassCore() {
		postProcess.EndSceneInputPass();
		return {};
	}

	GraphicsResult<void> OpenGlViewer::WaitIdle() {
		if (window == nullptr)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"wait for GPU", "the OpenGL window is unavailable"));
		glfwMakeContextCurrent(window);
		glFinish();
		return {};
	}

	GraphicsResult<void> OpenGlViewer::LoadPostProcessEffectsCore(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (!postProcess.Configure(screenWidth, screenHeight, capabilities.activeSampleCount, effects))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::EffectConfigurationFailed,
				"configure post-process effects", "the OpenGL effect chain could not be created"));
		return {};
	}

	std::unique_ptr<Instance> OpenGlViewer::CreateInstanceCore() {
		return std::make_unique<OpenGlInstance>(*this, textureCache, drawContext);
	}

}
