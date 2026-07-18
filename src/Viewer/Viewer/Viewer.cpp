#include "Viewer/Viewer/Viewer.h"

#include "Viewer/Instance/Instance.h"
#include "Viewer/PostProcess/PostProcess.h"

#include <algorithm>
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
		return MakeGraphicsError(graphicsApi, code, std::move(operation),
			std::move(message), nativeCode, hasNativeCode);
	}

	GraphicsResult<void> Viewer::RecreateSizeDependentResources(const int width, const int height, const bool force) {
		if (!initialized || rendererLost)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"렌더러 크기 변경", "렌더러를 사용할 수 없습니다"));
		if (width <= 0 || height <= 0)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"렌더러 크기 변경", "framebuffer 크기는 양수여야 합니다"));
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
				"렌더러 설정", "렌더러가 이미 초기화되었거나 사용 불가 상태입니다"));
		if (sourceWindow == nullptr || width <= 0 || height <= 0)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"렌더러 설정", "윈도우 또는 framebuffer 크기가 올바르지 않습니다"));
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
				"렌더러 설정", "API 구현이 post processor를 연결하지 않았습니다"));
		}
		initialized = true;
		return {};
	}

	GraphicsResult<void> Viewer::Resize(const int width, const int height) {
		if (frameActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"렌더러 크기 변경", "프레임을 기록하는 중에는 크기를 바꿀 수 없습니다"));
		return RecreateSizeDependentResources(width, height, false);
	}

	GraphicsResult<FrameBeginState> Viewer::BeginFrame() {
		if (!initialized || rendererLost || frameActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"프레임 시작", "렌더러를 사용할 수 없거나 이미 프레임 기록 중입니다"));
		const auto updateResult = ApplyPendingPostProcessParameterUpdates();
		if (!updateResult)
			return std::unexpected(updateResult.error());
		const auto result = BeginFrameCore();
		if (!result)
			return std::unexpected(result.error());
		frameActive = *result == FrameBeginState::Ready;
		return result;
	}

	GraphicsResult<FrameEndState> Viewer::EndFrame() {
		if (rendererLost || !frameActive || sceneInputPassActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"프레임 종료", "제출할 완성된 프레임이 없습니다"));
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
				"후처리 효과 구성", "렌더러를 사용할 수 없거나 프레임 기록 중입니다"));
		const auto waitResult = WaitIdle();
		if (!waitResult)
			return std::unexpected(waitResult.error());
		const auto loadResult = LoadPostProcessEffectsCore(effects);
		if (!loadResult)
			return std::unexpected(loadResult.error());
		pendingPostProcessParameterUpdates.clear();
		ResetPostProcessFrameHistory();
		return {};
	}

	GraphicsResult<void> Viewer::UpdatePostProcessParameters(
		const std::span<const EffectParameterUpdate> updates) {
		if (!initialized || rendererLost)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 파라미터 갱신", "렌더러를 사용할 수 없습니다"));
		if (activePostProcess == nullptr || !activePostProcess->ValidateParameterUpdates(updates))
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidArgument,
				"후처리 파라미터 갱신", "효과 색인, 슬롯 또는 값이 올바르지 않습니다"));
		for (const auto& update : updates) {
			const auto existing = std::ranges::find_if(pendingPostProcessParameterUpdates,
				[&update](const EffectParameterUpdate& pending) {
					return pending.effectIndex == update.effectIndex && pending.slot == update.slot;
				});
			if (existing == pendingPostProcessParameterUpdates.end())
				pendingPostProcessParameterUpdates.emplace_back(update);
			else
				existing->value = update.value;
		}
		return {};
	}

	GraphicsResult<void> Viewer::ApplyPendingPostProcessParameterUpdates() {
		if (pendingPostProcessParameterUpdates.empty())
			return {};
		if (activePostProcess == nullptr
			|| !activePostProcess->UpdateParameters(pendingPostProcessParameterUpdates)) {
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ContractViolation,
				"후처리 파라미터 적용", "예약된 파라미터와 현재 효과 구성이 일치하지 않습니다"));
		}
		pendingPostProcessParameterUpdates.clear();
		return {};
	}

	SceneDrawState Viewer::ResolveSceneDrawState() const {
		return {
			.scene = sceneRenderState,
			.previousViewMatrix = postProcessTemporalState.previousViewMatrix,
			.previousProjectionMatrix = postProcessTemporalState.previousProjectionMatrix,
			.screenSize = glm::vec2(screenWidth, screenHeight),
			.historyReset = postProcessTemporalState.historyResetPending,
			.velocityRequired = RequiresPostProcessVelocity()
		};
	}

	GraphicsResult<PostProcessSceneInputState> Viewer::BeginPostProcessSceneInputPass() {
		if (!frameActive || sceneInputPassActive)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"후처리 장면 입력 패스 시작", "프레임 또는 장면 입력 상태가 올바르지 않습니다"));
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
				"후처리 장면 입력 패스 종료", "장면 입력 패스가 활성화되지 않았습니다"));
		const auto result = EndPostProcessSceneInputPassCore();
		sceneInputPassActive = false;
		return result;
	}

	GraphicsResult<std::unique_ptr<Instance>> Viewer::CreateInstance(std::shared_ptr<Model> model,
		std::unique_ptr<Animation> animation, const float scale) {
		if (!initialized || rendererLost)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::InvalidState,
				"모델 인스턴스 생성", "렌더러를 사용할 수 없습니다"));
		auto instance = CreateInstanceCore();
		if (!instance)
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"모델 인스턴스 생성", "API별 인스턴스 객체를 만들지 못했습니다"));
		const auto initializeResult = instance->Initialize(
			std::move(model), std::move(animation), scale);
		if (!initializeResult)
			return std::unexpected(initializeResult.error());
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
				"framebuffer 리소스 재생성", "렌더러 윈도우를 사용할 수 없습니다"));
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		if (width <= 0 || height <= 0)
			return {};
		return RecreateSizeDependentResources(width, height, true);
	}

}
