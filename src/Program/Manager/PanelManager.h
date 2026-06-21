#pragma once

#include "../Config.h"
#include "../MenuBar.h"
#include "../PanelWindow.h"
#include "../RendererType.h"
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

		SceneConfig sceneConfigStorage;
		MenuBar menuBar;
		ModelPanel modelPanel;
		MotionPanel motionPanel;
		InterpolationCurvePanel interpolationCurvePanel;
		PlaybackPanel playbackPanel;
		SoundPanel soundPanel;
		PanelWindow panelWindow;

		// 현재 씬 설정의 모델 경로를 모델 패널 목록에 반영한다.
		void UpdateModelPanel();

	public:
		PanelManager();
		~PanelManager();

		SceneConfig& GetSceneConfig() { return sceneConfigStorage; }
		RendererType GetRendererType() const { return menuBar.GetRendererType(); }
		bool IsPhysicsEnabled() const { return menuBar.IsPhysicsEnabled(); }
		bool IsCameraMode() const { return motionPanel.GetMode() == MotionTimelineMode::Camera; }
		bool IsFpsVisible() const { return menuBar.IsFpsVisible(); }
		bool IsCloseRequested() const { return panelWindow.IsCloseRequested(); }

		void SetMotionMode(const MotionTimelineMode mode) { motionPanel.SetMode(mode); }
		void SetPlaybackFrame(const int frame) { motionPanel.SetCurrentFrame((std::max)(0, frame)); }
		PlaybackFrameRange GetPlaybackFrameRange() const { return playbackPanel.GetFrameRange(); }
		bool IsPlaybackRepeatEnabled() const { return playbackPanel.IsRepeatEnabled(); }

		void SetFrameLimits(const int autoLastFrame, const int motionLastFrame, const bool resetPlaybackRange = false) {
			playbackPanel.SetLastFrame(autoLastFrame, resetPlaybackRange);
			motionPanel.SetLastFrame(motionLastFrame);
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
		// 현재 씬 설정의 모델 목록을 패널에 다시 반영한다.
		void RefreshModelList() { UpdateModelPanel(); }
		// 선택 모델의 모션 트랙을 모션 패널에 표시한다.
		void SetMotionTimeline(std::wstring modelName, std::vector<MotionTimelineGroup> groups) {
			motionPanel.SetTimeline(std::move(modelName), std::move(groups));
		}
		// 사운드 패널이 조절할 사운드 객체를 연결한다.
		void BindSound(Sound& sound) { soundPanel.BindSound(sound); }
		// 메뉴와 패널의 일회성 변경 상태를 초기화한다.
		void Reset() { menuBar.Reset(); }
		// 렌더링 창이 아닌 GUI 창들을 생성하거나 다시 표시한다.
		bool OpenGuiWindows();
		// 현재 언어로 설정 창과 패널 컨트롤을 다시 생성한다.
		void RefreshLanguage() const;
		// 렌더링 창이 아닌 GUI 창들의 보류 중인 Win32 메시지를 처리한다.
		void PollGuiWindows();
		// GUI 창과 패널 컨트롤을 정리한다.
		void DestroyGui();
	};
}
