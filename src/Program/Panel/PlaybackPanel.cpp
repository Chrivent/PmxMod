#include "PlaybackPanel.h"

#include "../Manager/GuiManager.h"

#include <CommCtrl.h>

namespace Chrivent {
	LRESULT CALLBACK PlaybackPanel::WindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		auto* panel = reinterpret_cast<PlaybackPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (msg == WM_NCCREATE) {
			const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			panel = static_cast<PlaybackPanel*>(create->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
			panel->panelWindow = hwnd;
		}
		if (!panel)
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		switch (msg) {
			case WM_CREATE:
				panel->CreateContent(hwnd);
				return 0;
			case WM_SIZE: {
				RECT client{};
				GetClientRect(hwnd, &client);
				panel->Resize(client);
				return 0;
			}
			case WM_COMMAND:
				if (panel->HandleCommand(LOWORD(wParam)))
					return 0;
				break;
			case WM_HSCROLL:
				if (panel->HandleScroll(reinterpret_cast<HWND>(lParam), LOWORD(wParam)))
					return 0;
				break;
			case WM_CLOSE:
				ShowWindow(hwnd, SW_HIDE);
				return 0;
			case WM_DESTROY:
				panel->panelWindow = nullptr;
				panel->timelineSlider = nullptr;
				panel->playButton = nullptr;
				panel->pauseButton = nullptr;
				panel->stopButton = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void PlaybackPanel::SetControlIds(const int timelineId, const int playId, const int pauseId, const int stopId) {
		timelineSliderId = timelineId;
		playButtonId = playId;
		pauseButtonId = pauseId;
		stopButtonId = stopId;
	}

	void PlaybackPanel::Show() {
		if (panelWindow) {
			ShowWindow(panelWindow, SW_SHOWNORMAL);
			SetForegroundWindow(panelWindow);
			return;
		}
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = L"PmxModPlaybackPanel";
		RegisterClassExW(&wc);
		panelWindow = CreateWindowExW(
			0, L"PmxModPlaybackPanel", L"Playback",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 440, 150,
			nullptr, nullptr, instance, this);
		if (!panelWindow)
			return;
		ShowWindow(panelWindow, SW_SHOWNORMAL);
		UpdateWindow(panelWindow);
	}

	void PlaybackPanel::Poll() const {
		if (!panelWindow)
			return;
		MSG msg{};
		while (PeekMessageW(&msg, panelWindow, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	void PlaybackPanel::CreateContent(const HWND parent) {
		timelineSlider = GuiManager::CreateHorizontalSlider(parent, timelineSliderId, 0, 10000, 0);
		playButton = CreateWindowExW(
			0, L"BUTTON", L"Play",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(playButtonId)), GetModuleHandleW(nullptr), nullptr);
		pauseButton = CreateWindowExW(
			0, L"BUTTON", L"Pause",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(pauseButtonId)), GetModuleHandleW(nullptr), nullptr);
		stopButton = CreateWindowExW(
			0, L"BUTTON", L"Stop",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(stopButtonId)), GetModuleHandleW(nullptr), nullptr);
	}

	void PlaybackPanel::Resize(const RECT& clientRect) {
		constexpr int margin = 18;
		constexpr int sliderHeight = 36;
		constexpr int buttonWidth = 72;
		constexpr int buttonHeight = 28;
		constexpr int buttonGap = 8;
		const int width = (std::max)(0, static_cast<int>(clientRect.right) - margin * 2);
		constexpr int sliderY = 18;
		if (timelineSlider)
			MoveWindow(timelineSlider, margin, sliderY, width, sliderHeight, TRUE);
		constexpr int buttonTotalWidth = buttonWidth * 3 + buttonGap * 2;
		const int buttonX = (std::max)(margin, (static_cast<int>(clientRect.right) - buttonTotalWidth) / 2);
		constexpr int buttonY = sliderY + sliderHeight + 10;
		if (playButton)
			MoveWindow(playButton, buttonX, buttonY, buttonWidth, buttonHeight, TRUE);
		if (pauseButton)
			MoveWindow(pauseButton, buttonX + buttonWidth + buttonGap, buttonY, buttonWidth, buttonHeight, TRUE);
		if (stopButton)
			MoveWindow(stopButton, buttonX + (buttonWidth + buttonGap) * 2, buttonY, buttonWidth, buttonHeight, TRUE);
	}

	bool PlaybackPanel::HandleCommand(const int commandId) {
		if (commandId == playButtonId)
			pendingCommand = PlaybackCommand::Play;
		else if (commandId == pauseButtonId)
			pendingCommand = PlaybackCommand::Pause;
		else if (commandId == stopButtonId)
			pendingCommand = PlaybackCommand::Stop;
		else
			return false;
		return true;
	}

	bool PlaybackPanel::HandleScroll(const HWND control, const int scrollCode) {
		if (control != timelineSlider)
			return false;
		seekFrame = static_cast<int>(SendMessageW(timelineSlider, TBM_GETPOS, 0, 0));
		seekRequested = true;
		seekFinished = scrollCode == TB_ENDTRACK;
		return true;
	}

	void PlaybackPanel::Destroy() {
		if (panelWindow)
			DestroyWindow(panelWindow);
		panelWindow = nullptr;
		timelineSlider = nullptr;
		playButton = nullptr;
		pauseButton = nullptr;
		stopButton = nullptr;
	}

	PlaybackCommand PlaybackPanel::ConsumeCommand() {
		const PlaybackCommand command = pendingCommand;
		pendingCommand = PlaybackCommand::None;
		return command;
	}

	bool PlaybackPanel::ConsumeSeekFrame(int& frame, bool& finished) {
		if (!seekRequested)
			return false;
		frame = seekFrame;
		finished = seekFinished;
		seekRequested = false;
		seekFinished = false;
		return true;
	}

	void PlaybackPanel::SetCurrentFrame(const int frame) const {
		if (!timelineSlider)
			return;
		SendMessageW(timelineSlider, TBM_SETPOS, TRUE, static_cast<LPARAM>((std::max)(0, frame)));
	}

	void PlaybackPanel::SetFrameRange(const int maxFrame) const {
		if (!timelineSlider)
			return;
		const int safeMaxFrame = (std::max)(0, maxFrame);
		SendMessageW(timelineSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, safeMaxFrame));
		SendMessageW(timelineSlider, TBM_SETPOS, TRUE, 0);
	}
}
