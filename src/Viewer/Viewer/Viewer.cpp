#include "Viewer/Viewer/Viewer.h"

#include "Viewer/Instance/Instance.h"
#include "Viewer/PostProcess/PostProcess.h"

#include <utility>

namespace Chrivent {
	void Viewer::ConfigureWindowHints() {
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	}

	void Viewer::ResetPostProcessFrameHistory() {
		postProcessTemporalState.historyResetPending = true;
		postProcessTemporalState.frameData.historyReset = 1.0f;
	}

	void Viewer::CommitPostProcessFrameHistory() {
		postProcessTemporalState.previousViewMatrix = sceneRenderState.viewMatrix;
		postProcessTemporalState.previousProjectionMatrix = sceneRenderState.projectionMatrix;
		postProcessTemporalState.historyResetPending = false;
		postProcessTemporalState.frameData.historyReset = 0.0f;
	}

	bool Viewer::RecreateSizeDependentResources(const int width, const int height, const bool force) {
		if (!initialized || rendererLost || width <= 0 || height <= 0)
			return false;
		if (!force && screenWidth == width && screenHeight == height)
			return true;
		screenWidth = width;
		screenHeight = height;
		if (!ResizeCore()) {
			rendererLost = true;
			return false;
		}
		ResetPostProcessHistory();
		return true;
	}

	bool Viewer::Setup(GLFWwindow* sourceWindow, const int width, const int height,
		const SceneShaderRuntimeContract& shaderContract) {
		if (initialized || rendererLost || sourceWindow == nullptr || width <= 0 || height <= 0)
			return false;
		window = sourceWindow;
		screenWidth = width;
		screenHeight = height;
		if (!SetupCore(shaderContract) || activePostProcess == nullptr) {
			rendererLost = true;
			return false;
		}
		initialized = true;
		return true;
	}

	bool Viewer::Resize(const int width, const int height) {
		if (frameActive)
			return false;
		return RecreateSizeDependentResources(width, height, false);
	}

	FrameBeginResult Viewer::BeginFrame() {
		if (!initialized || rendererLost || frameActive)
			return FrameBeginResult::Failed;
		const FrameBeginResult result = BeginFrameCore();
		frameActive = result == FrameBeginResult::Ready;
		return result;
	}

	FrameEndResult Viewer::EndFrame() {
		if (rendererLost || !frameActive || sceneInputPassActive)
			return FrameEndResult::Failed;
		const FrameEndResult result = EndFrameCore();
		frameActive = false;
		PostProcess& postProcess = *activePostProcess;
		if (result == FrameEndResult::Presented) {
			postProcess.CommitHistoryFrame();
			CommitPostProcessFrameHistory();
		} else
			postProcess.DiscardHistoryFrame();
		return result;
	}

	void Viewer::UpdatePostProcessFrameData(PostProcessFrameData frameData) {
		postProcessTemporalState.frameData = std::move(frameData);
	}

	bool Viewer::LoadPostProcessEffects(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (!initialized || rendererLost || frameActive || !WaitIdle() || !LoadPostProcessEffectsCore(effects))
			return false;
		ResetPostProcessFrameHistory();
		return true;
	}

	PostProcessSceneInputBeginResult Viewer::BeginPostProcessSceneInputPass() {
		if (!frameActive || sceneInputPassActive)
			return PostProcessSceneInputBeginResult::Failed;
		const PostProcess& postProcess = *activePostProcess;
		if (!postProcess.RequiresDepth() && !postProcess.RequiresVelocity())
			return PostProcessSceneInputBeginResult::NotRequired;
		if (!BeginPostProcessSceneInputPassCore())
			return PostProcessSceneInputBeginResult::Failed;
		sceneInputPassActive = true;
		return PostProcessSceneInputBeginResult::Ready;
	}

	bool Viewer::EndPostProcessSceneInputPass() {
		if (!sceneInputPassActive)
			return false;
		const bool result = EndPostProcessSceneInputPassCore();
		sceneInputPassActive = false;
		return result;
	}

	std::unique_ptr<Instance> Viewer::CreateInstance(std::shared_ptr<Model> model,
		std::unique_ptr<Animation> animation, const float scale) {
		if (!initialized || rendererLost)
			return nullptr;
		auto instance = CreateInstanceCore();
		if (!instance || !instance->Initialize(std::move(model), std::move(animation), scale))
			return nullptr;
		return instance;
	}

	void Viewer::ResetPostProcessHistory() {
		if (activePostProcess != nullptr)
			activePostProcess->ResetHistory();
		ResetPostProcessFrameHistory();
	}

	bool Viewer::RequiresPostProcessVelocity() const {
		return activePostProcess != nullptr && activePostProcess->RequiresVelocity();
	}

	bool Viewer::RecreateFromFramebuffer() {
		if (window == nullptr)
			return false;
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		if (width <= 0 || height <= 0)
			return true;
		return RecreateSizeDependentResources(width, height, true);
	}

}
