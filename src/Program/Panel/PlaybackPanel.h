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
		bool seekFinished = false;
		int seekFrame = 0;
		HWND panelWindow = nullptr;
		HWND timelineSlider = nullptr;
		HWND playButton = nullptr;
		HWND pauseButton = nullptr;
		HWND stopButton = nullptr;

		// 플레이백 패널 윈도우의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 패널 안에 타임라인 슬라이더와 재생 제어 버튼을 만든다.
		void CreateContent(HWND parent);

	public:
		PlaybackPanel() = default;

		void SetControlIds(const int timelineId, const int playId, const int pauseId, const int stopId) {
			timelineSliderId = timelineId;
			playButtonId = playId;
			pauseButtonId = pauseId;
			stopButtonId = stopId;
		}
		
		// 패널 윈도우를 생성하거나 이미 있으면 다시 표시한다.
		void Show();
		// 패널 윈도우에 쌓인 메시지를 처리한다.
		void Poll() const;
		// 패널 크기에 맞춰 슬라이더와 버튼 위치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 재생 제어 버튼 명령을 내부 재생 명령으로 저장한다.
		bool HandleCommand(int commandId) override;
		// 타임라인 슬라이더 이동을 프레임 이동 요청으로 저장한다.
		bool HandleScroll(HWND control, int scrollCode) override;
		// 패널 윈도우와 컨트롤 핸들을 정리한다.
		void Destroy() override;
		// 버튼으로 들어온 재생 명령을 반환하고 내부 상태를 초기화한다.
		PlaybackCommand ConsumeCommand();
		// 슬라이더로 요청된 이동 프레임을 반환하고 내부 상태를 초기화한다.
		bool ConsumeSeekFrame(int& frame, bool& finished);
		// 현재 재생 프레임을 슬라이더 위치에 반영한다.
		void SetCurrentFrame(int frame) const;
		// 슬라이더가 이동할 수 있는 마지막 프레임을 설정한다.
		void SetFrameRange(int maxFrame) const;
	};
}
