#pragma once

#include <windows.h>

#include "../Config.h"
#include "../Panel/PlaybackPanel.h"
#include "../Panel/SoundPanel.h"
#include "../../Viewer/ViewerMenu.h"

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
		ViewerMenu viewerMenu;
		PlaybackPanel playbackPanel;
		SoundPanel soundPanel;
		HWND renderWindow = nullptr;
		WNDPROC prevRenderWindowProc = nullptr;

		// 렌더링 창의 Win32 메시지를 받아 뷰어 메뉴 명령을 처리한다.
		static LRESULT CALLBACK RenderWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	public:
		PanelManager();
		~PanelManager();
		
		SceneConfig& GetSceneConfig() { return sceneConfigStorage; }
		void SetPlaybackFrame(const int frame) const { playbackPanel.SetCurrentFrame(frame); }
		void SetPlaybackFrameRange(const int maxFrame) const { playbackPanel.SetFrameRange(maxFrame); }
		void ApplySceneConfig(const SceneConfig& cfg) { viewerMenu.ApplySceneConfig(cfg); }
		bool ConsumeSceneConfigDirty() { return viewerMenu.ConsumeSceneConfigDirty(); }
		PlaybackCommand ConsumePlaybackCommand() { return playbackPanel.ConsumeCommand(); }
		bool ConsumeSeekFrame(int& frame, bool& finished) { return playbackPanel.ConsumeSeekFrame(frame, finished); }
		void BindSound(Sound& sound) { soundPanel.BindSound(sound); }
		void Reset() { viewerMenu.Reset(); }

		// 렌더링 창에 상단 메뉴를 연결한다.
		void AttachRenderWindow(const ViewerInfo& viewerInfo);
		// 렌더링 창이 아닌 GUI 창들을 생성하거나 다시 표시한다.
		bool OpenGuiWindows();
		// 렌더링 창이 아닌 GUI 창들의 보류 중인 Win32 메시지를 처리한다.
		void PollGuiWindows() const;
		// GUI 창과 렌더링 창 메뉴 연결을 정리한다.
		void DestroyGui();
	};
}
