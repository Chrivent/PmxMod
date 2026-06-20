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
		int playButton = 0;
		int pauseButton = 0;
		int stopButton = 0;
		int startFrameEdit = 0;
		int endFrameEdit = 0;
		int resetRangeButton = 0;
	};

	class PlaybackPanel final : public Panel {
		static constexpr int kMaxEditableFrame = 65535;

		PlaybackControlIds controlIds;
		PlaybackCommand pendingCommand = PlaybackCommand::None;
		bool customFrameRange = false;
		bool timelineRangeChanged = false;
		bool updatingRangeControls = false;
		int autoLastFrame = 1;
		PlaybackFrameRange frameRange;
		HWND panelWindow = nullptr;
		HWND playButton = nullptr;
		HWND pauseButton = nullptr;
		HWND stopButton = nullptr;
		HWND startFrameEdit = nullptr;
		HWND rangeSeparatorText = nullptr;
		HWND endFrameEdit = nullptr;
		HWND resetRangeButton = nullptr;

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
		// 패널 안에 재생 제어 버튼과 재생 범위 입력 칸을 만든다.
		void CreateContent(HWND parent);
		// 수정한 입력 칸을 기준으로 시작과 끝 프레임의 순서를 보정한다.
		void ApplyInputFrameRange(int editedControlId);
		// 지정한 재생 범위를 입력 컨트롤에 반영한다.
		void ApplyFrameRange(PlaybackFrameRange range, bool customRange);
		// 시작과 끝 프레임을 입력 가능한 범위와 순서에 맞게 보정한다.
		static PlaybackFrameRange NormalizeFrameRange(PlaybackFrameRange range);
		// 재생 범위를 입력 컨트롤에 반영한다.
		void UpdateRangeControls();

	public:
		PlaybackPanel() = default;

		PlaybackFrameRange GetFrameRange() const { return frameRange; }

		void SetControlIds(const PlaybackControlIds& ids) { controlIds = ids; }
		
		// Auto 버튼으로 복원할 마지막 프레임을 설정한다.
		void SetLastFrame(int maxFrame);
		// 부모 윈도우 아래에 패널 컨트롤을 생성한다.
		void Create(HWND parent) override;
		// 패널 윈도우를 생성하거나 이미 있으면 다시 표시한다.
		void Show();
		// 패널 윈도우에 쌓인 메시지를 처리한다.
		void Poll() const;
		// 패널 크기에 맞춰 버튼과 입력 칸 위치를 갱신한다.
		void Resize(const RECT& clientRect) override;
		// 재생 제어 버튼 명령을 내부 재생 명령으로 저장한다.
		bool HandleCommand(int commandId, int notificationCode) override;
		// 패널 윈도우와 컨트롤 핸들을 정리한다.
		void Destroy() override;
		// 버튼으로 들어온 재생 명령을 반환하고 내부 상태를 초기화한다.
		PlaybackCommand ConsumeCommand();
		// 변경된 사용자 재생 범위를 반환하고 변경 상태를 초기화한다.
		bool ConsumeTimelineRangeChange(PlaybackFrameRange& range);
	};
}
