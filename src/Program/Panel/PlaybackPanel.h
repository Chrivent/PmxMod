#pragma once

#include "Panel.h"

namespace Chrivent {
	enum class PlaybackCommand {
		None,
		Play,
		Pause,
		Stop
	};

	class PlaybackPanel final : public Panel {
		int timelineSliderId = 0;
		int playButtonId = 0;
		int pauseButtonId = 0;
		int stopButtonId = 0;
		PlaybackCommand pendingCommand = PlaybackCommand::None;
		bool seekRequested = false;
		int seekFrame = 0;
		HWND panelWindow = nullptr;
		HWND timelineSlider = nullptr;
		HWND playButton = nullptr;
		HWND pauseButton = nullptr;
		HWND stopButton = nullptr;

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 패널 안에 타임라인 슬라이더와 재생 제어 버튼을 만든다.
		void CreateContent(HWND parent);

	public:
		PlaybackPanel() = default;

		// GUI 매니저가 관리하는 컨트롤 ID를 패널에 전달한다.
		void SetControlIds(int timelineId, int playId, int pauseId, int stopId);
		void Show();
		void Poll() const;
		void Resize(const RECT& clientRect) override;
		bool HandleCommand(int commandId) override;
		bool HandleScroll(HWND control) override;
		void Destroy() override;
		// 버튼으로 들어온 재생 명령을 반환하고 내부 상태를 초기화한다.
		PlaybackCommand ConsumeCommand();
		// 슬라이더로 요청된 이동 프레임을 반환하고 내부 상태를 초기화한다.
		bool ConsumeSeekFrame(int& frame);
		// 현재 재생 프레임을 슬라이더 위치에 반영한다.
		void SetCurrentFrame(int frame) const;
	};
}
