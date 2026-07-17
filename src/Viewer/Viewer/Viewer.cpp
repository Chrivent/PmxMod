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

	bool Viewer::Setup(GLFWwindow* sourceWindow, const int width, const int height,
		SceneShaderRuntimeContract shaderContract) {
		if (initialized || sourceWindow == nullptr || width <= 0 || height <= 0)
			return false;
		window = sourceWindow;
		screenWidth = width;
		screenHeight = height;
		builtInShaderPasses = std::move(shaderContract.builtIn);
		sceneInputShaderPasses = std::move(shaderContract.sceneInput);
		if (!SetupCore() || activePostProcess == nullptr)
			return false;
		initialized = true;
		return true;
	}

	bool Viewer::Resize(const int width, const int height) {
		if (!initialized || frameActive || width <= 0 || height <= 0)
			return false;
		if (screenWidth == width && screenHeight == height)
			return true;
		screenWidth = width;
		screenHeight = height;
		if (!ResizeCore())
			return false;
		ResetPostProcessHistory();
		return true;
	}

	FrameBeginResult Viewer::BeginFrame() {
		if (!initialized || frameActive)
			return FrameBeginResult::Failed;
		const FrameBeginResult result = BeginFrameCore();
		frameActive = result == FrameBeginResult::Ready;
		return result;
	}

	FrameEndResult Viewer::EndFrame() {
		if (!frameActive || sceneInputPassActive)
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
		if (!initialized || frameActive || !WaitIdle() || !LoadPostProcessEffectsCore(effects))
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
		auto instance = CreateInstanceCore();
		if (!instance || !instance->Initialize(std::move(model), std::move(animation), scale))
			return nullptr;
		return instance;
	}

	void Viewer::ResetPostProcessHistory() {
		activePostProcess->ResetHistory();
		ResetPostProcessFrameHistory();
	}

	bool Viewer::RequiresPostProcessVelocity() const {
		return activePostProcess->RequiresVelocity();
	}

}
