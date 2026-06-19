#pragma once

#include "Panel.h"

namespace Chrivent {
	enum class PlaybackCommand {
		None,
		Play,
		Pause,
		Stop
	};

	struct PlaybackFrameRange {
		int start = 0;
		int end = 0;
	};

	struct PlaybackControlIds {
		int timelineSlider = 0;
		int playButton = 0;
		int pauseButton = 0;
		int stopButton = 0;
		int startFrameEdit = 0;
		int endFrameEdit = 0;
		int resetRangeButton = 0;
		int totalFrameEdit = 0;
	};

	class PlaybackPanel final : public Panel {
		PlaybackControlIds controlIds;
		PlaybackCommand pendingCommand = PlaybackCommand::None;
		bool seekRequested = false;
		bool seekFinished = false;
		bool customFrameRange = false;
		bool customTotalFrame = false;
		bool timelineRangeChanged = false;
		bool updatingRangeControls = false;
		int seekFrame = 0;
		int defaultLastFrame = 0;
		int totalFrame = 1;
		PlaybackFrameRange frameRange;
		HWND panelWindow = nullptr;
		HWND timelineSlider = nullptr;
		HWND playButton = nullptr;
		HWND pauseButton = nullptr;
		HWND stopButton = nullptr;
		HWND startFrameEdit = nullptr;
		HWND rangeSeparatorText = nullptr;
		HWND endFrameEdit = nullptr;
		HWND resetRangeButton = nullptr;
		HWND totalFrameEdit = nullptr;

		// 플레이백 패널 윈도우의 Win32 메시지를 처리한다.
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		// 숫자 입력 칸에서 누른 Enter를 입력값 확정으로 처리한다.
		static LRESULT CALLBACK EditWindowProc(
			HWND hwnd,
			UINT msg,
			WPARAM wParam,
			LPARAM lParam,
			UINT_PTR subclassId,
			DWORD_PTR data);
		// 패널 안에 타임라인 슬라이더와 재생 제어 버튼을 만든다.
		void CreateContent(HWND parent);
		// 수정한 입력 칸을 기준으로 시작과 끝 프레임의 순서를 보정한다.
		void ApplyInputFrameRange(int editedControlId);
		// 전체 프레임 입력값을 타임라인 길이로 적용한다.
		void ApplyInputTotalFrame();
		// 지정한 재생 범위를 슬라이더와 입력 컨트롤에 반영한다.
		void ApplyFrameRange(PlaybackFrameRange range, bool customRange);
		// 시작과 끝 프레임을 전체 프레임 안의 유효한 범위로 보정한다.
		PlaybackFrameRange NormalizeFrameRange(PlaybackFrameRange range) const;
		// 전체 프레임과 재생 범위를 모든 입력 컨트롤에 반영한다.
		void UpdateRangeControls();

	public:
		PlaybackPanel() = default;

		PlaybackFrameRange GetFrameRange() const { return frameRange; }
		int GetTotalFrame() const { return totalFrame; }

		void SetControlIds(const PlaybackControlIds& ids) { controlIds = ids; }
		
		// 현재 재생 프레임을 슬라이더 위치에 반영한다.
		void SetCurrentFrame(int frame) const;
		// 슬라이더가 이동할 수 있는 마지막 프레임을 설정한다.
		void SetFrameRange(int maxFrame);
		// 부모 윈도우 아래에 패널 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 윈도우를 생성하거나 이미 있으면 다시 표시한다.
		void Show();
		// 패널 윈도우에 쌓인 메시지를 처리한다.
		void Poll() const;
		// 패널 크기에 맞춰 슬라이더와 버튼 위치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 재생 제어 버튼 명령을 내부 재생 명령으로 저장한다.
		bool HandleCommand(int commandId, int notificationCode) override;
		// 타임라인 슬라이더 이동을 프레임 이동 요청으로 저장한다.
		bool HandleScroll(HWND control, int scrollCode) override;
		// 패널 윈도우와 컨트롤 핸들을 정리한다.
		void Destroy() override;
		// 버튼으로 들어온 재생 명령을 반환하고 내부 상태를 초기화한다.
		PlaybackCommand ConsumeCommand();
		// 슬라이더로 요청된 이동 프레임을 반환하고 내부 상태를 초기화한다.
		bool ConsumeSeekFrame(int& frame, bool& finished);
		// 변경된 사용자 재생 범위와 전체 프레임을 반환하고 변경 상태를 초기화한다.
		bool ConsumeTimelineRangeChange(PlaybackFrameRange& range, int& total);
	};
}
