#pragma once

#include <windows.h>

#include "../Config.h"
#include "../Panel/PlaybackPanel.h"
#include "../Panel/SoundPanel.h"
#include "../../Viewer/ViewerMenu.h"

namespace Chrivent {
	class Sound;
	class Viewer;

	class GuiManager {
		static constexpr int kPlaybackTimelineSliderId = 1001;
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
		SceneConfig& sceneConfig;

		GuiManager();
		~GuiManager();

		// 렌더링 창에 상단 메뉴를 연결한다.
		void AttachRenderWindow(const Viewer& viewer);
		// 외부에서 전달된 설정을 메뉴 상태에 반영한다.
		void ApplySceneConfig(const SceneConfig& cfg);
		// 메뉴와 GUI 패널 상태를 기본값으로 초기화한다.
		void Reset();
		// 렌더링 창이 아닌 GUI 창들을 생성하거나 다시 표시한다.
		bool OpenGuiWindows();
		// 렌더링 창이 아닌 GUI 창들의 보류 중인 Win32 메시지를 처리한다.
		void PollGuiWindows() const;
		// GUI 창과 렌더링 창 메뉴 연결을 정리한다.
		void DestroyGui();
		// 씬 설정 변경 플래그를 반환하고 초기화한다.
		bool ConsumeSceneConfigDirty();
		// 사운드 패널이 조절할 사운드 객체를 연결한다.
		void BindSound(Sound& sound);
		// 눈금 없는 가로 슬라이더 컨트롤을 생성한다.
		static HWND CreateHorizontalSlider(HWND parent, int controlId, int minValue, int maxValue, int initialValue);
		// 눈금이 있는 세로 슬라이더 컨트롤을 생성한다.
		static HWND CreateVerticalTickSlider(HWND parent, int controlId, int minValue, int maxValue, int initialValue, int tickFrequency);
	};
}
