#include "PlaybackPanel.h"

#include "../Language.h"

#include <CommCtrl.h>
#include <algorithm>
#include <string>

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
			if (panel->HandleCommand(LOWORD(wParam), HIWORD(wParam)))
				return 0;
			break;
		case WM_CLOSE:
			ShowWindow(hwnd, SW_HIDE);
			return 0;
		case WM_DESTROY:
			panel->panelWindow = nullptr;
			panel->playButton = nullptr;
			panel->pauseButton = nullptr;
			panel->stopButton = nullptr;
			panel->startFrameEdit = nullptr;
			panel->rangeSeparatorText = nullptr;
			panel->endFrameEdit = nullptr;
			panel->resetRangeButton = nullptr;
			panel->repeatCheck = nullptr;
			return 0;
		default:
			break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	LRESULT CALLBACK PlaybackPanel::EditWindowProc(
		const HWND hwnd,
		const UINT msg,
		const WPARAM wParam,
		const LPARAM lParam,
		const UINT_PTR subclassId,
		const DWORD_PTR data) {
		if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
			auto* panel = reinterpret_cast<PlaybackPanel*>(data);
			const int controlId = GetDlgCtrlID(hwnd);
			panel->ApplyInputFrameRange(controlId);
			return 0;
		}
		if (msg == WM_NCDESTROY)
			RemoveWindowSubclass(hwnd, EditWindowProc, subclassId);
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	void PlaybackPanel::CreateContent(const HWND parent) {
		if (playButton)
			return;
		playButton = CreateWindowExW(
			0, L"BUTTON", Language::Text("playback.play").c_str(),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.playButton)), GetModuleHandleW(nullptr), nullptr);
		pauseButton = CreateWindowExW(
			0, L"BUTTON", Language::Text("playback.pause").c_str(),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.pauseButton)), GetModuleHandleW(nullptr), nullptr);
		stopButton = CreateWindowExW(
			0, L"BUTTON", Language::Text("playback.stop").c_str(),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.stopButton)), GetModuleHandleW(nullptr), nullptr);
		startFrameEdit = CreateWindowExW(
			WS_EX_CLIENTEDGE, L"EDIT", L"0",
			WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT | ES_AUTOHSCROLL,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.startFrameEdit)), GetModuleHandleW(nullptr), nullptr);
		rangeSeparatorText = CreateWindowExW(
			0, L"STATIC", L"~",
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			0, 0, 0, 0,
			parent, nullptr, GetModuleHandleW(nullptr), nullptr);
		endFrameEdit = CreateWindowExW(
			WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(frameRange.end).c_str(),
			WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT | ES_AUTOHSCROLL,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.endFrameEdit)), GetModuleHandleW(nullptr), nullptr);
		resetRangeButton = CreateWindowExW(
			0, L"BUTTON", Language::Text("playback.auto").c_str(),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.resetRangeButton)), GetModuleHandleW(nullptr), nullptr);
		repeatCheck = CreateWindowExW(
			0, L"BUTTON", Language::Text("playback.repeat").c_str(),
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.repeatCheck)), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(repeatCheck, BM_SETCHECK, repeatEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
		SendMessageW(startFrameEdit, EM_SETLIMITTEXT, 10, 0);
		SendMessageW(endFrameEdit, EM_SETLIMITTEXT, 10, 0);
		SetWindowSubclass(startFrameEdit, EditWindowProc, controlIds.startFrameEdit, reinterpret_cast<DWORD_PTR>(this));
		SetWindowSubclass(endFrameEdit, EditWindowProc, controlIds.endFrameEdit, reinterpret_cast<DWORD_PTR>(this));
		ApplyFrameRange(frameRange, customFrameRange);
	}

	void PlaybackPanel::ApplyInputFrameRange(const int editedControlId) {
		if (updatingRangeControls)
			return;
		wchar_t startText[16]{};
		wchar_t endText[16]{};
		GetWindowTextW(startFrameEdit, startText, std::size(startText));
		GetWindowTextW(endFrameEdit, endText, std::size(endText));
		if (startText[0] == L'\0' || endText[0] == L'\0')
			return;
		int start = _wtoi(startText);
		int end = _wtoi(endText);
		if (editedControlId == controlIds.startFrameEdit) {
			start = std::clamp(start, 0, kMaxEditableFrame - 1);
			end = std::clamp((std::max)(end, start + 1), start + 1, kMaxEditableFrame);
		} else {
			end = std::clamp(end, 1, kMaxEditableFrame);
			start = std::clamp((std::min)(start, end - 1), 0, end - 1);
		}
		ApplyFrameRange({start, end}, true);
		timelineRangeChanged = true;
	}

	void PlaybackPanel::ApplyFrameRange(const PlaybackFrameRange range, const bool customRange) {
		frameRange = NormalizeFrameRange(range);
		customFrameRange = customRange;
		UpdateRangeControls();
	}

	PlaybackFrameRange PlaybackPanel::NormalizeFrameRange(PlaybackFrameRange range) {
		range.start = std::clamp(range.start, 0, kMaxEditableFrame - 1);
		range.end = std::clamp(range.end, range.start + 1, kMaxEditableFrame);
		return range;
	}

	void PlaybackPanel::UpdateRangeControls() {
		updatingRangeControls = true;
		if (startFrameEdit)
			SetWindowTextW(startFrameEdit, std::to_wstring(frameRange.start).c_str());
		if (endFrameEdit)
			SetWindowTextW(endFrameEdit, std::to_wstring(frameRange.end).c_str());
		updatingRangeControls = false;
	}

	void PlaybackPanel::SetLastFrame(const int maxFrame) {
		autoLastFrame = std::clamp(maxFrame, 1, kMaxEditableFrame);
		if (!customFrameRange)
			ApplyFrameRange({ 0, autoLastFrame }, false);
		else
			ApplyFrameRange(frameRange, true);
	}

	void PlaybackPanel::Create(const HWND parent) {
		CreateContent(parent);
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
			0, L"PmxModPlaybackPanel", Language::Text("panel.playback").c_str(),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 440, 110,
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

	void PlaybackPanel::Resize(const RECT& clientRect) {
		constexpr int buttonWidth = 72;
		constexpr int buttonHeight = 28;
		constexpr int buttonGap = 8;
		constexpr int frameEditWidth = 66;
		constexpr int separatorWidth = 24;
		constexpr int resetButtonWidth = 64;
		constexpr int repeatWidth = 74;
		constexpr int buttonTotalWidth = buttonWidth * 3 + buttonGap * 2;
		constexpr int rangeWidth = frameEditWidth * 2 + separatorWidth + buttonGap + resetButtonWidth;
		const int clientWidth = clientRect.right - clientRect.left;
		const int buttonX = clientRect.left + (clientWidth - buttonTotalWidth) / 2;
		const int buttonY = clientRect.top + 12;
		if (playButton)
			MoveWindow(playButton, buttonX, buttonY, buttonWidth, buttonHeight, TRUE);
		if (pauseButton)
			MoveWindow(pauseButton, buttonX + buttonWidth + buttonGap, buttonY, buttonWidth, buttonHeight, TRUE);
		if (stopButton)
			MoveWindow(stopButton, buttonX + (buttonWidth + buttonGap) * 2, buttonY, buttonWidth, buttonHeight, TRUE);
		const int rangeX = clientRect.left + (clientWidth - rangeWidth) / 2;
		const int rangeY = buttonY + buttonHeight + 12;
		if (startFrameEdit)
			MoveWindow(startFrameEdit, rangeX, rangeY, frameEditWidth, buttonHeight, TRUE);
		if (rangeSeparatorText)
			MoveWindow(rangeSeparatorText, rangeX + frameEditWidth, rangeY + 4, separatorWidth, buttonHeight, TRUE);
		if (endFrameEdit)
			MoveWindow(endFrameEdit, rangeX + frameEditWidth + separatorWidth, rangeY, frameEditWidth, buttonHeight, TRUE);
		if (resetRangeButton)
			MoveWindow(resetRangeButton, rangeX + frameEditWidth * 2 + separatorWidth + buttonGap,
				rangeY, resetButtonWidth, buttonHeight, TRUE);
		if (repeatCheck)
			MoveWindow(repeatCheck, clientRect.left + (clientWidth - repeatWidth) / 2,
				rangeY + buttonHeight + 8, repeatWidth, buttonHeight, TRUE);
	}

	void PlaybackPanel::UpdateLanguage() {
		if (playButton)
			SetWindowTextW(playButton, Language::Text("playback.play").c_str());
		if (pauseButton)
			SetWindowTextW(pauseButton, Language::Text("playback.pause").c_str());
		if (stopButton)
			SetWindowTextW(stopButton, Language::Text("playback.stop").c_str());
		if (resetRangeButton)
			SetWindowTextW(resetRangeButton, Language::Text("playback.auto").c_str());
		if (repeatCheck)
			SetWindowTextW(repeatCheck, Language::Text("playback.repeat").c_str());
	}

	bool PlaybackPanel::HandleCommand(const int commandId, const int notificationCode) {
		if (commandId == controlIds.playButton)
			pendingCommand = PlaybackCommand::Play;
		else if (commandId == controlIds.pauseButton)
			pendingCommand = PlaybackCommand::Pause;
		else if (commandId == controlIds.stopButton)
			pendingCommand = PlaybackCommand::Stop;
		else if ((commandId == controlIds.startFrameEdit || commandId == controlIds.endFrameEdit) &&
			notificationCode == EN_KILLFOCUS)
			ApplyInputFrameRange(commandId);
		else if (commandId == controlIds.resetRangeButton) {
			ApplyFrameRange({0, autoLastFrame}, false);
			timelineRangeChanged = true;
		}
		else if (commandId == controlIds.repeatCheck)
			repeatEnabled = SendMessageW(repeatCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
		else
			return false;
		return true;
	}

	void PlaybackPanel::Destroy() {
		if (panelWindow)
			DestroyWindow(panelWindow);
		if (playButton)
			DestroyWindow(playButton);
		if (pauseButton)
			DestroyWindow(pauseButton);
		if (stopButton)
			DestroyWindow(stopButton);
		if (startFrameEdit)
			DestroyWindow(startFrameEdit);
		if (rangeSeparatorText)
			DestroyWindow(rangeSeparatorText);
		if (endFrameEdit)
			DestroyWindow(endFrameEdit);
		if (resetRangeButton)
			DestroyWindow(resetRangeButton);
		if (repeatCheck)
			DestroyWindow(repeatCheck);
		panelWindow = nullptr;
		playButton = nullptr;
		pauseButton = nullptr;
		stopButton = nullptr;
		startFrameEdit = nullptr;
		rangeSeparatorText = nullptr;
		endFrameEdit = nullptr;
		resetRangeButton = nullptr;
		repeatCheck = nullptr;
	}

	PlaybackCommand PlaybackPanel::ConsumeCommand() {
		const PlaybackCommand command = pendingCommand;
		pendingCommand = PlaybackCommand::None;
		return command;
	}

	bool PlaybackPanel::ConsumeTimelineRangeChange(PlaybackFrameRange& range) {
		if (!timelineRangeChanged)
			return false;
		range = frameRange;
		timelineRangeChanged = false;
		return true;
	}
}
