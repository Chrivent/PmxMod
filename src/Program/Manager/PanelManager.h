#pragma once

#include "../Config.h"
#include "../MenuBar.h"
#include "../PanelWindow.h"
#include "../RendererType.h"
#include "../Panel/CameraPanel.h"
#include "../Panel/InterpolationCurvePanel.h"
#include "../Panel/ModelPanel.h"
#include "../Panel/MotionPanel.h"
#include "../Panel/PlaybackPanel.h"
#include "../Panel/SoundPanel.h"

#include <algorithm>
#include <utility>

namespace Chrivent {
	class Sound;

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
		static constexpr UINT_PTR kModelListId = 3002;
		static constexpr UINT_PTR kCameraShaderListId = 4001;

		SceneConfig sceneConfigStorage;
		MenuBar menuBar;
		ModelPanel modelPanel;
		CameraPanel cameraPanel;
		MotionPanel motionPanel;
		InterpolationCurvePanel interpolationCurvePanel;
		PlaybackPanel playbackPanel;
		SoundPanel soundPanel;
		PanelWindow panelWindow;

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
		void ApplyMotionMode(const MotionTimelineMode mode) {
			motionPanel.ChangeMode(mode);
			UpdateSidePanelVisibility();
		}
		void SetPlaybackFrame(const int frame) { motionPanel.UpdateCurrentFrame((std::max)(0, frame)); }
		void SetRendererType(const RendererType rendererType) { menuBar.ApplyRenderer(rendererType); }
		PlaybackFrameRange GetPlaybackFrameRange() const { return playbackPanel.GetFrameRange(); }
		bool IsPlaybackRepeatEnabled() const { return playbackPanel.IsRepeatEnabled(); }

		// Auto 범위와 모션 스크롤의 마지막 프레임을 함께 갱신한다.
		void UpdateFrameLimits(const int autoLastFrame, const int motionLastFrame, const bool resetPlaybackRange = false) {
			playbackPanel.UpdateLastFrame(autoLastFrame, resetPlaybackRange);
			motionPanel.UpdateLastFrame(motionLastFrame);
		}
		// 실제 재생 상태를 메뉴와 편집 패널의 활성화 상태에 반영한다.
		void ApplyPlaybackState(const bool playing) {
			menuBar.ApplyPlaybackState(playing);
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
		// 언어 변경 여부를 반환하고 내부 플래그를 초기화한다.
		bool ConsumeLanguageDirty() { return menuBar.ConsumeLanguageDirty(); }
		// 대기 중인 재생 명령을 반환하고 내부 상태를 초기화한다.
		PlaybackCommand ConsumePlaybackCommand() { return playbackPanel.ConsumeCommand(); }
		// 대기 중인 프레임 이동 요청을 반환하고 내부 상태를 초기화한다.
		bool ConsumeSeekFrame(int& frame, bool& finished) { return motionPanel.ConsumeSeekFrame(frame, finished); }
		// 모델 패널에서 선택한 PMX 경로를 반환하고 대기 요청을 초기화한다.
		bool ConsumeAddModelPath(std::filesystem::path& modelPath) { return modelPanel.ConsumeAddModelPath(modelPath); }
		// 모델 패널에서 선택한 모델 인덱스를 반환하고 대기 요청을 초기화한다.
		bool ConsumeSelectedModelIndex(size_t& modelIndex) { return modelPanel.ConsumeSelectedModelIndex(modelIndex); }
		// 카메라 패널에서 선택한 셰이더 인덱스를 반환하고 대기 요청을 초기화한다.
		bool ConsumeSelectedShaderIndex(size_t& shaderIndex) { return cameraPanel.ConsumeSelectedShaderIndex(shaderIndex); }
		// 현재 씬 설정의 모델 목록을 패널에 다시 반영한다.
		void RefreshModelList() { UpdateModelPanel(); }
		// 검색된 셰이더 효과 이름과 현재 선택을 카메라 패널에 반영한다.
		void ApplyShaderNames(const std::vector<std::wstring>& names, const size_t selectedIndex) {
			cameraPanel.UpdateShaderNames(names, selectedIndex);
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
		// 현재 언어로 설정 창과 패널 컨트롤을 다시 생성한다.
		void RefreshLanguage() const { panelWindow.RefreshLanguage(); }
		// 렌더링 창이 아닌 GUI 창들의 보류 중인 Win32 메시지를 처리한다.
		void PollGuiWindows();
		// GUI 창과 패널 컨트롤을 정리한다.
		void DestroyGui() { panelWindow.Destroy(); }
	};
}
