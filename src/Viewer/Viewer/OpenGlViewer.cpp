#include "Viewer/Viewer/OpenGlViewer.h"

#include "Viewer/Instance/OpenGlInstance.h"

#include <utility>

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

	void OpenGlViewer::ConfigureWindowHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SAMPLES, msaaSamples);
	}

	GraphicsError::Result<void> OpenGlViewer::SetupCore(const SceneShaderRuntimeContract& shaderContract) {
		BindPostProcess(postProcess);
		const auto deviceResult = OpenGlDevice::Initialize(window, msaaSamples, capabilities);
		if (!deviceResult)
			return std::unexpected(deviceResult.error());
		const auto pipelineResult = pipeline.Initialize(shaderContract);
		if (!pipelineResult)
			return std::unexpected(pipelineResult.error());
		const auto dummyTextureResult = textureCache.CreateWhiteTexture();
		if (!dummyTextureResult)
			return std::unexpected(dummyTextureResult.error());
		drawContext.SetDummyColorTexture(dummyTextureResult->texture);
		glViewport(0, 0, screenWidth, screenHeight);
		return {};
	}

	GraphicsError::Result<void> OpenGlViewer::ResizeCore() {
		glViewport(0, 0, screenWidth, screenHeight);
		if (postProcess.HasEffects()) {
			const auto postProcessResult = postProcess.InitializeTargets(
				screenWidth, screenHeight, capabilities.activeSampleCount);
			if (!postProcessResult)
				return std::unexpected(postProcessResult.error());
			return {};
		}
		postProcess.ResetResources();
		return {};
	}

	GraphicsError::Result<FrameBeginState> OpenGlViewer::BeginFrameCore() {
		drawContext.BeginFrame();
		glBindFramebuffer(GL_FRAMEBUFFER, postProcess.ResolveSceneFramebuffer());
		glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		return FrameBeginState::Ready;
	}

	GraphicsError::Result<FrameEndState> OpenGlViewer::EndFrameCore() {
		const auto drawResult = postProcess.Draw(screenWidth, screenHeight, GetPostProcessFrameData());
		if (!drawResult)
			return std::unexpected(drawResult.error());
		glfwSwapBuffers(window);
		return FrameEndState::Presented;
	}

	GraphicsError::Result<void> OpenGlViewer::BeginPostProcessSceneInputPassCore() {
		return postProcess.BeginSceneInputPass(screenWidth, screenHeight);
	}

	GraphicsError::Result<void> OpenGlViewer::EndPostProcessSceneInputPassCore() {
		return postProcess.EndSceneInputPass();
	}

	GraphicsError::Result<void> OpenGlViewer::WaitIdleCore() {
		return OpenGlDevice::WaitIdle(window);
	}

	GraphicsError::Result<void> OpenGlViewer::LoadPostProcessEffectsCore(
		PreparedPostProcessEffects preparedEffects) {
		return postProcess.Configure(screenWidth, screenHeight,
			capabilities.activeSampleCount, std::move(preparedEffects));
	}

	std::unique_ptr<Instance> OpenGlViewer::CreateInstanceCore() {
		return std::make_unique<OpenGlInstance>(textureCache, drawContext);
	}

}
