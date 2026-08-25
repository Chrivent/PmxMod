#pragma once

#include "Program/Config.h"
#include "Program/MenuBar.h"
#include "Program/PanelWindow.h"
#include "Program/RendererType.h"
#include "Program/Panel/CameraPanel.h"
#include "Program/Panel/InformationPanel.h"
#include "Program/Panel/InterpolationCurvePanel.h"
#include "Program/Panel/ModelPanel.h"
#include "Program/Panel/MotionPanel.h"
#include "Program/Panel/PlaybackPanel.h"
#include "Program/Panel/SoundPanel.h"

#include <algorithm>
#include <functional>
#include <utility>

namespace Chrivent {
	class Sound;

	// 프로그램 패널과 보조 창을 생성하고 상호 상태를 동기화한다.
	class PanelManager {
		static constexpr UINT_PTR kPlaybackPlayButtonId = 1002;
		static constexpr UINT_PTR kPlaybackPauseButtonId = 1003;
		static constexpr UINT_PTR kPlaybackStopButtonId = 1004;
		static constexpr UINT_PTR kPlaybackStartFrameEditId = 1005;
		static constexpr UINT_PTR kPlaybackEndFrameEditId = 1006;
		static constexpr UINT_PTR kPlaybackResetRangeButtonId = 1007;
		static constexpr UINT_PTR kPlaybackRepeatCheckId = 1008;
		static constexpr UINT_PTR kSoundVolumeSliderId = 2001;
		static constexpr UINT_PTR kModelAddButtonId = 3001;
		static constexpr UINT_PTR kModelDeleteButtonId = 3002;
		static constexpr UINT_PTR kModelListId = 3003;
		static constexpr UINT_PTR kCameraAddMotionButtonId = 5001;
		static constexpr UINT_PTR kCameraDeleteMotionButtonId = 5002;
		static constexpr UINT_PTR kCameraShaderListId = 5003;
		static constexpr UINT_PTR kCameraModelShaderCheckId = 5004;
		static constexpr UINT_PTR kCameraEdgeShaderCheckId = 5005;
		static constexpr UINT_PTR kCameraGroundShadowShaderCheckId = 5006;

		SceneConfig sceneConfigStorage;
		MenuBar menuBar;
		ModelPanel modelPanel;
		CameraPanel cameraPanel;
		InformationPanel informationPanel;
		MotionPanel motionPanel;
		InterpolationCurvePanel interpolationCurvePanel;
		PlaybackPanel playbackPanel;
		SoundPanel soundPanel;
		PanelWindow panelWindow;
		MotionTimelineMode visibleMotionMode = MotionTimelineMode::Camera;

		// 현재 씬 설정의 모델 경로를 모델 패널 목록에 반영한다.
		void UpdateModelPanel();
		// 모션 타임라인 모드에 맞춰 모델과 카메라 패널을 교체한다.
		void UpdateSidePanelVisibility();

	public:
		PanelManager();
		~PanelManager();

		SceneConfig& GetSceneConfig() { return sceneConfigStorage; }
		RendererType GetRendererType() const { return menuBar.GetRendererType(); }
		bool IsPhysicsEnabled() const { return menuBar.IsPhysicsEnabled(); }
		bool IsCameraMode() const { return motionPanel.GetMode() == MotionTimelineMode::Camera; }
		bool IsFpsVisible() const { return menuBar.IsFpsVisible(); }
		bool IsCloseRequested() const { return panelWindow.IsCloseRequested(); }

		// 모션 표시 모드를 바꾸고 좌측 패널 표시를 함께 갱신한다.
		void ApplyMotionMode(MotionTimelineMode mode);
		void SetPlaybackFrame(const int frame) { motionPanel.UpdateCurrentFrame(std::max(0, frame)); }
		void SetRendererType(const RendererType rendererType) { menuBar.ApplyRenderer(rendererType); }
		PlaybackFrameRange GetPlaybackFrameRange() const { return playbackPanel.GetFrameRange(); }
		bool IsPlaybackRepeatEnabled() const { return playbackPanel.IsRepeatEnabled(); }

		// Auto 범위와 타임라인 스크롤의 마지막 프레임을 함께 갱신한다.
		void UpdateFrameLimits(const int autoLastFrame, const int motionLastFrame, const bool resetPlaybackRange = false) {
			playbackPanel.UpdateLastFrame(autoLastFrame, resetPlaybackRange);
			motionPanel.UpdateLastFrame(std::max(autoLastFrame, motionLastFrame));
		}
		// 실제 재생 상태를 메뉴와 편집 패널의 활성화 상태에 반영한다.
		void ApplyPlaybackState(const bool playing) {
			menuBar.ApplyPlaybackState(playing);
			modelPanel.ApplyPlaybackState(playing);
			cameraPanel.ApplyPlaybackState(playing);
			playbackPanel.ApplyPlaybackState(playing);
			motionPanel.ApplyPlaybackState(playing);
		}

		// 외부에서 전달된 씬 설정을 메뉴와 내부 저장소에 반영한다.
		void ApplySceneConfig(const SceneConfig& cfg);
		// 성공적으로 로드한 씬 설정을 내부 저장소와 모델 목록에 확정한다.
		void CommitSceneConfig(const SceneConfig& cfg);
		// 씬 설정 변경 여부를 반환하고 내부 플래그를 초기화한다.
		bool ConsumeSceneConfigDirty() { return menuBar.ConsumeSceneConfigDirty(); }
		// 렌더러 변경 여부를 반환하고 내부 플래그를 초기화한다.
		bool ConsumeRendererDirty() { return menuBar.ConsumeRendererDirty(); }
		// 물리 활성화 변경 여부를 반환하고 내부 플래그를 초기화한다.
		bool ConsumePhysicsDirty() { return menuBar.ConsumePhysicsDirty(); }
		// 언어 변경 여부를 반환하고 내부 플래그를 초기화한다.
		bool ConsumeLanguageDirty() { return menuBar.ConsumeLanguageDirty(); }
		// 대기 중인 재생 명령을 반환하고 내부 상태를 초기화한다.
		PlaybackCommand ConsumePlaybackCommand() { return playbackPanel.ConsumeCommand(); }
		// 대기 중인 프레임 이동 요청을 반환하고 내부 상태를 초기화한다.
		bool ConsumeSeekFrame(int& frame, bool& finished) { return motionPanel.ConsumeSeekFrame(frame, finished); }
		// 모델 패널에서 선택한 PMX 경로를 반환하고 대기 요청을 초기화한다.
		bool ConsumeAddModelPath(std::filesystem::path& modelPath) { return modelPanel.ConsumeAddModelPath(modelPath); }
		// 모델 패널에서 삭제할 모델 인덱스를 반환하고 대기 요청을 초기화한다.
		bool ConsumeDeleteModelIndex(size_t& modelIndex) { return modelPanel.ConsumeDeleteModelIndex(modelIndex); }
		// 모델 패널에서 선택한 모델 모션 경로를 반환하고 대기 요청을 초기화한다.
		bool ConsumeModelMotionPath(size_t& modelIndex, std::filesystem::path& motionPath) {
			return modelPanel.ConsumeModelMotionPath(modelIndex, motionPath);
		}
		// 모델 패널에서 선택한 모델 인덱스를 반환하고 대기 요청을 초기화한다.
		bool ConsumeSelectedModelIndex(size_t& modelIndex) { return modelPanel.ConsumeSelectedModelIndex(modelIndex); }
		// 카메라 패널에서 선택한 셰이더 인덱스를 반환하고 대기 요청을 초기화한다.
		bool ConsumeSelectedShaderIndex(size_t& shaderIndex, bool& enabled) { return cameraPanel.ConsumeSelectedShaderIndex(shaderIndex, enabled); }
		// 카메라 패널에서 변경한 내장 장면 셰이더 체크 상태를 반환한다.
		bool ConsumeBuiltInShaderToggle(BuiltInShaderToggle& shader, bool& enabled) {
			return cameraPanel.ConsumeBuiltInShaderToggle(shader, enabled);
		}
		// 카메라 패널의 카메라 모션 행 선택 요청을 반환하고 대기 요청을 초기화한다.
		bool ConsumeCameraMotionSelected() { return cameraPanel.ConsumeCameraMotionSelected(); }
		// 카메라 패널에서 선택한 카메라 모션 경로를 반환하고 대기 요청을 초기화한다.
		bool ConsumeCameraMotionPath(std::filesystem::path& motionPath) { return cameraPanel.ConsumeCameraMotionPath(motionPath); }
		// 카메라 패널의 카메라 모션 삭제 요청을 반환하고 대기 요청을 초기화한다.
		bool ConsumeDeleteCameraMotion() { return cameraPanel.ConsumeDeleteCameraMotion(); }
		// 현재 씬 설정의 모델 목록을 패널에 다시 반영한다.
		void RefreshModelList() { UpdateModelPanel(); }
		// 검색된 셰이더 효과 이름과 현재 선택을 카메라 패널에 반영한다.
		void ApplyShaderNames(const std::vector<std::wstring>& names, const size_t selectedIndex, const std::vector<bool>& enabledStates) {
			cameraPanel.UpdateShaderNames(names, selectedIndex, enabledStates);
		}
		// 내장 모델·엣지·지면 그림자 셰이더의 체크 상태를 카메라 패널에 반영한다.
		void ApplyBuiltInShaderStates(const bool modelEnabled, const bool edgeEnabled, const bool groundShadowEnabled) {
			cameraPanel.UpdateBuiltInShaderStates(modelEnabled, edgeEnabled, groundShadowEnabled);
		}
		// 선택한 모델이나 이펙트의 정보 필드를 좌측 정보 패널에 표시한다.
		void ApplyInformation(std::vector<InformationField> fields);
		// 선택 정보가 없을 때 정보 패널을 비우고 숨긴다.
		void ClearInformation();
		// 현재 씬의 카메라 모션 경로를 카메라 패널에 반영한다.
		void ApplyCameraMotionPath(const std::filesystem::path& motionPath) {
			cameraPanel.UpdateCameraMotionPath(motionPath);
		}
		// 선택 모델의 이름과 모션 타임라인을 패널에 적용한다.
		void ApplyMotionTimeline(std::wstring modelName, std::vector<MotionTimelineGroup> groups) {
			motionPanel.ApplyTimeline(std::move(modelName), std::move(groups));
		}
		// 사운드 패널이 조절할 사운드 객체를 연결한다.
		void BindSound(Sound& sound);
		// 메뉴와 패널의 일회성 변경 상태를 초기화한다.
		void Reset() { menuBar.Reset(); }
		// 렌더링 창이 아닌 GUI 창들을 생성하거나 다시 표시한다.
		bool OpenGuiWindows();
		// 창 이동과 패널 경계 드래그 종료 콜백을 연결한다.
		void SetInteractionFinishedCallback(std::function<void()> callback) {
			panelWindow.SetInteractionFinishedCallback(std::move(callback));
		}
		// 메뉴바 모달 루프 중 렌더링을 유지할 콜백을 연결한다.
		void SetMenuFrameCallback(std::function<bool()> callback) {
			panelWindow.SetMenuFrameCallback(std::move(callback));
		}
		// 현재 언어로 설정 창과 패널 컨트롤을 다시 생성한다.
		void RefreshLanguage() const { panelWindow.RefreshLanguage(); }
		// 렌더링 창이 아닌 GUI 창들의 보류 중인 Win32 메시지를 처리한다.
		void PollGuiWindows();
		// GUI 창과 패널 컨트롤을 정리한다.
		void DestroyGui() { panelWindow.Destroy(); }
	};
}
