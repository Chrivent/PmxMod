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

	GraphicsError Viewer::CreateGraphicsError(const GraphicsErrorCode code, std::string operation,
		std::string message, const int64_t nativeCode, const bool hasNativeCode) const {
		return GraphicsError{
			.api = graphicsApi,
			.code = code,
			.operation = std::move(operation),
			.message = std::move(message),
			.nativeCode = nativeCode,
			.hasNativeCode = hasNativeCode
		};
	}

	GraphicsResult<void> Viewer::RecreateSizeDependentResources(const int width, const int height, const bool force) {
		if (!initialized || rendererLost)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"resize renderer", "the renderer is not available"));
		if (width <= 0 || height <= 0)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"resize renderer", "the framebuffer size must be positive"));
		if (!force && screenWidth == width && screenHeight == height)
			return {};
		screenWidth = width;
		screenHeight = height;
		const auto resizeResult = ResizeCore();
		if (!resizeResult) {
			rendererLost = true;
			return std::unexpected(resizeResult.error());
		}
		ResetPostProcessHistory();
		return {};
	}

	GraphicsResult<void> Viewer::Setup(GLFWwindow* sourceWindow, const int width, const int height,
		const SceneShaderRuntimeContract& shaderContract) {
		if (initialized || rendererLost)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"set up renderer", "the renderer has already been initialized or lost"));
		if (sourceWindow == nullptr || width <= 0 || height <= 0)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"set up renderer", "the window and framebuffer size are invalid"));
		window = sourceWindow;
		screenWidth = width;
		screenHeight = height;
		const auto setupResult = SetupCore(shaderContract);
		if (!setupResult) {
			rendererLost = true;
			return std::unexpected(setupResult.error());
		}
		if (activePostProcess == nullptr) {
			rendererLost = true;
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ContractViolation,
				"set up renderer", "the API implementation did not bind a post processor"));
		}
		initialized = true;
		return {};
	}

	GraphicsResult<void> Viewer::Resize(const int width, const int height) {
		if (frameActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"resize renderer", "a frame is currently being recorded"));
		return RecreateSizeDependentResources(width, height, false);
	}

	GraphicsResult<FrameBeginState> Viewer::BeginFrame() {
		if (!initialized || rendererLost || frameActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"begin frame", "the renderer is unavailable or a frame is already active"));
		const auto result = BeginFrameCore();
		if (!result)
			return std::unexpected(result.error());
		frameActive = *result == FrameBeginState::Ready;
		return result;
	}

	GraphicsResult<FrameEndState> Viewer::EndFrame() {
		if (rendererLost || !frameActive || sceneInputPassActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"end frame", "no complete frame is ready for submission"));
		const auto result = EndFrameCore();
		frameActive = false;
		PostProcess& postProcess = *activePostProcess;
		if (!result) {
			postProcess.DiscardHistoryFrame();
			return std::unexpected(result.error());
		}
		if (*result == FrameEndState::Presented) {
			postProcess.CommitHistoryFrame();
			CommitPostProcessFrameHistory();
		} else
			postProcess.DiscardHistoryFrame();
		return result;
	}

	void Viewer::UpdatePostProcessFrameData(PostProcessFrameData frameData) {
		postProcessTemporalState.frameData = std::move(frameData);
	}

	GraphicsResult<void> Viewer::LoadPostProcessEffects(const std::vector<const EffectRuntimeDefinition*>& effects) {
		if (!initialized || rendererLost || frameActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"configure post-process effects", "the renderer is unavailable or a frame is active"));
		const auto waitResult = WaitIdle();
		if (!waitResult)
			return std::unexpected(waitResult.error());
		const auto loadResult = LoadPostProcessEffectsCore(effects);
		if (!loadResult)
			return std::unexpected(loadResult.error());
		ResetPostProcessFrameHistory();
		return {};
	}

	GraphicsResult<PostProcessSceneInputState> Viewer::BeginPostProcessSceneInputPass() {
		if (!frameActive || sceneInputPassActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"begin post-process scene input pass", "the frame or scene input state is invalid"));
		const PostProcess& postProcess = *activePostProcess;
		if (!postProcess.RequiresDepth() && !postProcess.RequiresVelocity())
			return PostProcessSceneInputState::NotRequired;
		const auto beginResult = BeginPostProcessSceneInputPassCore();
		if (!beginResult)
			return std::unexpected(beginResult.error());
		sceneInputPassActive = true;
		return PostProcessSceneInputState::Ready;
	}

	GraphicsResult<void> Viewer::EndPostProcessSceneInputPass() {
		if (!sceneInputPassActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"end post-process scene input pass", "the scene input pass is not active"));
		const auto result = EndPostProcessSceneInputPassCore();
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

	GraphicsResult<void> Viewer::RecreateFromFramebuffer() {
		if (window == nullptr)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"recreate framebuffer resources", "the renderer window is unavailable"));
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		if (width <= 0 || height <= 0)
			return {};
		return RecreateSizeDependentResources(width, height, true);
	}

}
