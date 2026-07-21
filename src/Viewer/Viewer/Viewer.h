#pragma once

#include <memory>
#include <span>
#include <vector>
#include <glm/glm.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Viewer/Device/GraphicsCapabilities.h"
#include "Viewer/Drawer/SceneDrawState.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/PostProcess/PostProcess.h"
#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/Shader/SceneShaderRuntimeContract.h"

namespace Chrivent {
	class Animation;
	class Instance;
	class Model;

	// 프레임 기록 시작의 정상 상태를 준비 완료와 일시적 건너뜀으로 구분한다.
	enum class FrameBeginState {
		Ready,
		Skipped
	};

	// 프레임 표시의 정상 상태를 표시 완료와 일시적 건너뜀으로 구분한다.
	enum class FrameEndState {
		Presented,
		Skipped
	};

	// 후처리 장면 입력 패스의 정상 상태를 준비 완료와 불필요로 구분한다.
	enum class PostProcessSceneInputState {
		Ready,
		NotRequired
	};

	// 렌더링 API 구현이 따라야 할 장면 렌더링과 후처리 공통 계약을 정의한다.
	class Viewer {
		// 시간 기반 후처리의 이전 카메라 상태와 현재 프레임 입력을 한 단위로 보관한다.
		struct PostProcessTemporalState {
			glm::mat4 previousViewMatrix{1.0f};
			glm::mat4 previousProjectionMatrix{1.0f};
			PostProcessFrameData frameData;
			bool historyResetPending = true;
		};
		
		PostProcessTemporalState postProcessTemporalState;
		SceneRenderState sceneRenderState;
		std::vector<EffectParameterUpdate> pendingPostProcessParameterUpdates;
		PostProcess* activePostProcess = nullptr;
		bool initialized = false;
		bool rendererLost = false;
		bool frameActive = false;
		bool sceneInputPassActive = false;
		bool invertNdcYForTextureCoordinates = false;
		GraphicsApi graphicsApi = GraphicsApi::Unknown;
		
		// 표시가 끝난 카메라 행렬을 다음 프레임의 이전 상태로 확정한다.
		void CommitPostProcessFrameHistory();
		// UI에서 예약한 후처리 파라미터 변경을 새 프레임 경계에서 적용한다.
		GraphicsError::Result<void> ApplyPendingPostProcessParameterUpdates();
		// 복구할 수 없는 API 오류 뒤 공통 프레임 상태를 정리하고 렌더러를 사용 불가로 전환한다.
		void InvalidateRenderer();
		// 새 크기로 API별 리소스를 재생성하고 실패하면 렌더러를 사용 불가 상태로 전환한다.
		GraphicsError::Result<void> RecreateSizeDependentResources(int width, int height, bool force);

	protected:
		explicit Viewer(const GraphicsApi sourceGraphicsApi, const bool invertNdcY) :
			invertNdcYForTextureCoordinates(invertNdcY), graphicsApi(sourceGraphicsApi) {}

		float clearColor[4] = { 0.839f, 0.902f, 0.961f, 1.0f };
		int screenWidth = 0;
		int screenHeight = 0;
		GLFWwindow* window = nullptr;
		GraphicsCapabilities capabilities;

		// API별 리소스로 검증된 후처리 실행 체인을 생성한다.
		virtual GraphicsError::Result<void> LoadPostProcessEffectsCore(
			PreparedPostProcessEffects preparedEffects) = 0;
		// API별 후처리 장면 입력 패스 기록을 시작한다.
		virtual GraphicsError::Result<void> BeginPostProcessSceneInputPassCore() = 0;
		// API별 후처리 장면 입력 패스 기록을 종료한다.
		virtual GraphicsError::Result<void> EndPostProcessSceneInputPassCore() = 0;
		// API별 렌더러 리소스를 초기화한다.
		virtual GraphicsError::Result<void> SetupCore(const SceneShaderRuntimeContract& shaderContract) = 0;
		// API별 크기 의존 렌더링 리소스를 갱신한다.
		virtual GraphicsError::Result<void> ResizeCore() = 0;
		// API별 한 프레임 기록을 시작한다.
		virtual GraphicsError::Result<FrameBeginState> BeginFrameCore() = 0;
		// API별 프레임 제출과 화면 표시 결과를 반환한다.
		virtual GraphicsError::Result<FrameEndState> EndFrameCore() = 0;
		// API별 GPU 작업이 모두 끝날 때까지 기다린다.
		virtual GraphicsError::Result<void> WaitIdleCore() = 0;
		// 현재 렌더러에 맞는 초기 상태의 모델 인스턴스를 생성한다.
		virtual std::unique_ptr<Instance> CreateInstanceCore() = 0;
		// API 구현이 소유한 포스트 프로세서를 공통 프레임 계약에 연결한다.
		void BindPostProcess(PostProcess& postProcess) { activePostProcess = &postProcess; }
		// 현재 API 정보와 작업 문맥을 포함한 구조화된 그래픽 오류를 생성한다.
		GraphicsError CreateGraphicsError(GraphicsErrorCode code, std::string operation,
			std::string message, int64_t nativeCode = 0, bool hasNativeCode = false) const;
		// 다음 프레임에서 시간 기반 후처리 입력을 현재 상태로 초기화한다.
		void ResetPostProcessFrameHistory();
		// 현재 GLFW framebuffer 크기로 API별 리소스를 재생성하며 최소화 상태에서는 다음 프레임으로 미룬다.
		GraphicsError::Result<void> RecreateFromFramebuffer();

	public:
		virtual ~Viewer() = default;

		Viewer(const Viewer&) = delete;
		Viewer& operator=(const Viewer&) = delete;

		GLFWwindow* GetWindow() const { return window; }
		int GetScreenWidth() const { return screenWidth; }
		int GetScreenHeight() const { return screenHeight; }
		SceneRenderState& GetSceneRenderState() { return sceneRenderState; }
		const GraphicsCapabilities& GetGraphicsCapabilities() const { return capabilities; }
		bool IsNdcYInvertedForTextureCoordinates() const {
			return invertNdcYForTextureCoordinates;
		}
		const glm::mat4& GetPreviousViewMatrix() const {
			return postProcessTemporalState.previousViewMatrix;
		}
		const PostProcessFrameData& GetPostProcessFrameData() const {
			return postProcessTemporalState.frameData;
		}
		bool IsPostProcessHistoryResetPending() const {
			return postProcessTemporalState.historyResetPending;
		}

		// 렌더러별 GLFW 윈도우 힌트를 설정한다.
		virtual void ConfigureWindowHints();
		// 윈도우, 크기와 검증된 장면 셰이더 계약을 받은 뒤 렌더러 리소스를 한 번 초기화한다.
		GraphicsError::Result<void> Setup(GLFWwindow* sourceWindow, int width, int height,
			const SceneShaderRuntimeContract& shaderContract);
		// 창 크기에 맞춰 렌더 타깃과 투영 행렬을 갱신한다.
		GraphicsError::Result<void> Resize(int width, int height);
		// 한 프레임의 렌더링 시작 상태를 준비하고 기록 가능 여부를 반환한다.
		GraphicsError::Result<FrameBeginState> BeginFrame();
		// 한 프레임을 제출하고 표시 결과에 맞춰 시간 기반 히스토리를 확정한다.
		GraphicsError::Result<FrameEndState> EndFrame();
		// 후처리 요구 입력을 확인하고 장면 depth와 velocity 입력 패스를 시작한다.
		GraphicsError::Result<PostProcessSceneInputState> BeginPostProcessSceneInputPass();
		// 후처리 장면 입력 패스를 종료하고 기록 성공 여부를 반환한다.
		GraphicsError::Result<void> EndPostProcessSceneInputPass();
		// 렌더러가 제출한 GPU 작업이 모두 끝날 때까지 기다리고 실패 시 공통 상태를 정리한다.
		GraphicsError::Result<void> WaitIdle();
		// 체크된 포스트 프로세스 효과의 선언형 리소스와 패스 그래프를 렌더러에 준비한다.
		// HLSL 입력은 FrameData=b0, 패스별 JSON reads=t0~t7, LinearClamp=s0 규격을 사용한다.
		GraphicsError::Result<void> LoadPostProcessEffects(const std::vector<const EffectRuntimeDefinition*>& effects);
		// 활성 후처리 효과의 스칼라 파라미터 변경을 다음 프레임에 적용하도록 예약한다.
		GraphicsError::Result<void> UpdatePostProcessParameters(std::span<const EffectParameterUpdate> updates);
		// 현재 프레임의 Drawer 입력을 API 객체 수명과 분리된 값으로 조립한다.
		SceneDrawState ResolveSceneDrawState() const;
		// 모델 데이터가 완전히 초기화된 현재 렌더러용 인스턴스를 생성한다.
		GraphicsError::Result<std::unique_ptr<Instance>> CreateInstance(std::shared_ptr<Model> model,
			std::unique_ptr<Animation> animation, float scale);
		// 카메라 관리자가 계산한 현재 프레임 후처리 입력을 저장한다.
		void UpdatePostProcessFrameData(PostProcessFrameData frameData);
		// 카메라 점프나 탐색 뒤 다음 프레임의 temporal history를 초기화한다.
		void ResetPostProcessHistory();
		// 활성 후처리 효과가 장면 속도 입력을 요구하는지 반환한다.
		bool RequiresPostProcessVelocity() const;
	};
}
