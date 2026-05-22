#pragma once

#include "../Config.h"
#include "../MenuBar.h"
#include "../PanelWindow.h"
#include "../Panel/ModelPanel.h"
#include "../Panel/MotionPanel.h"
#include "../Panel/PlaybackPanel.h"
#include "../Panel/SoundPanel.h"

namespace Chrivent {
	class Sound;
	struct ViewerInfo;

	class PanelManager {
		static constexpr int kPlaybackTimelineSliderId = 1001;
		static constexpr int kPlaybackPlayButtonId = 1002;
		static constexpr int kPlaybackPauseButtonId = 1003;
		static constexpr int kPlaybackStopButtonId = 1004;
		static constexpr int kSoundVolumeSliderId = 2001;

		SceneConfig sceneConfigStorage;
		MenuBar menuBar;
		ModelPanel modelPanel;
		MotionPanel motionPanel;
		PlaybackPanel playbackPanel;
		SoundPanel soundPanel;
		PanelWindow panelWindow;
		HWND renderWindow = nullptr;

	public:
		PanelManager();
		~PanelManager();
		
		SceneConfig& GetSceneConfig() { return sceneConfigStorage; }
		void SetPlaybackFrame(const int frame) const { playbackPanel.SetCurrentFrame(frame); }
		void SetPlaybackFrameRange(const int maxFrame) const { playbackPanel.SetFrameRange(maxFrame); }
		void ApplySceneConfig(const SceneConfig& cfg) { menuBar.ApplySceneConfig(cfg); }
		bool ConsumeSceneConfigDirty() { return menuBar.ConsumeSceneConfigDirty(); }
		int GetRendererType() const { return menuBar.GetRendererType(); }
		bool ConsumeRendererDirty() { return menuBar.ConsumeRendererDirty(); }
		PlaybackCommand ConsumePlaybackCommand() { return playbackPanel.ConsumeCommand(); }
		bool ConsumeSeekFrame(int& frame, bool& finished) { return playbackPanel.ConsumeSeekFrame(frame, finished); }
		void BindSound(Sound& sound) { soundPanel.BindSound(sound); }
		void Reset() { menuBar.Reset(); }

		// 렌더링 창 핸들을 보관한다.
		void AttachRenderWindow(const ViewerInfo& viewerInfo);
		// 렌더링 창이 아닌 GUI 창들을 생성하거나 다시 표시한다.
		bool OpenGuiWindows();
		// 렌더링 창이 아닌 GUI 창들의 보류 중인 Win32 메시지를 처리한다.
		void PollGuiWindows() const;
		// GUI 창과 렌더링 창 메뉴 연결을 정리한다.
		void DestroyGui();
	};
}
