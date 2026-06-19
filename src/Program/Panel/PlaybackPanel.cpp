#include "PlaybackPanel.h"

#include "../GuiDrawer.h"

#include <CommCtrl.h>
#include <algorithm>

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
			panel->startFrameEdit = nullptr;
			panel->rangeSeparatorText = nullptr;
			panel->endFrameEdit = nullptr;
			panel->resetRangeButton = nullptr;
			panel->totalFrameEdit = nullptr;
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
			if (controlId == panel->controlIds.totalFrameEdit)
				panel->ApplyInputTotalFrame();
			else
				panel->ApplyInputFrameRange(controlId);
			return 0;
		}
		if (msg == WM_NCDESTROY)
			RemoveWindowSubclass(hwnd, EditWindowProc, subclassId);
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	void PlaybackPanel::CreateContent(const HWND parent) {
		if (timelineSlider)
			return;
		timelineSlider = GuiDrawer::CreateHorizontalSlider(parent, controlIds.timelineSlider, 0, 10000, 0);
		playButton = CreateWindowExW(
			0, L"BUTTON", L"Play",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.playButton)), GetModuleHandleW(nullptr), nullptr);
		pauseButton = CreateWindowExW(
			0, L"BUTTON", L"Pause",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.pauseButton)), GetModuleHandleW(nullptr), nullptr);
		stopButton = CreateWindowExW(
			0, L"BUTTON", L"Stop",
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
			0, L"BUTTON", L"Reset",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.resetRangeButton)), GetModuleHandleW(nullptr), nullptr);
		totalFrameEdit = CreateWindowExW(
			WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(totalFrame).c_str(),
			WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_RIGHT | ES_AUTOHSCROLL,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlIds.totalFrameEdit)), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(startFrameEdit, EM_SETLIMITTEXT, 10, 0);
		SendMessageW(endFrameEdit, EM_SETLIMITTEXT, 10, 0);
		SendMessageW(totalFrameEdit, EM_SETLIMITTEXT, 10, 0);
		SetWindowSubclass(startFrameEdit, EditWindowProc, controlIds.startFrameEdit, reinterpret_cast<DWORD_PTR>(this));
		SetWindowSubclass(endFrameEdit, EditWindowProc, controlIds.endFrameEdit, reinterpret_cast<DWORD_PTR>(this));
		SetWindowSubclass(totalFrameEdit, EditWindowProc, controlIds.totalFrameEdit, reinterpret_cast<DWORD_PTR>(this));
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
			start = std::clamp(start, 0, totalFrame - 1);
			end = std::clamp((std::max)(end, start + 1), start + 1, totalFrame);
		} else {
			end = std::clamp(end, 1, totalFrame);
			start = std::clamp((std::min)(start, end - 1), 0, end - 1);
		}
		ApplyFrameRange({start, end}, true);
		timelineRangeChanged = true;
	}

	void PlaybackPanel::ApplyInputTotalFrame() {
		if (updatingRangeControls)
			return;
		wchar_t text[16]{};
		GetWindowTextW(totalFrameEdit, text, std::size(text));
		if (text[0] == L'\0')
			return;
		totalFrame = (std::max)(1, _wtoi(text));
		customTotalFrame = true;
		ApplyFrameRange(frameRange, customFrameRange);
		timelineRangeChanged = true;
	}

	void PlaybackPanel::ApplyFrameRange(const PlaybackFrameRange range, const bool customRange) {
		frameRange = NormalizeFrameRange(range);
		customFrameRange = customRange;
		if (timelineSlider) {
			SendMessageW(timelineSlider, TBM_SETRANGEMIN, FALSE, 0);
			SendMessageW(timelineSlider, TBM_SETRANGEMAX, TRUE, totalFrame);
		}
		UpdateRangeControls();
	}

	PlaybackFrameRange PlaybackPanel::NormalizeFrameRange(PlaybackFrameRange range) const {
		range.start = std::clamp(range.start, 0, totalFrame - 1);
		range.end = std::clamp(range.end, range.start + 1, totalFrame);
		return range;
	}

	void PlaybackPanel::UpdateRangeControls() {
		updatingRangeControls = true;
		if (startFrameEdit)
			SetWindowTextW(startFrameEdit, std::to_wstring(frameRange.start).c_str());
		if (endFrameEdit)
			SetWindowTextW(endFrameEdit, std::to_wstring(frameRange.end).c_str());
		if (totalFrameEdit)
			SetWindowTextW(totalFrameEdit, std::to_wstring(totalFrame).c_str());
		updatingRangeControls = false;
	}

	void PlaybackPanel::SetCurrentFrame(const int frame) const {
		if (!timelineSlider)
			return;
		SendMessageW(timelineSlider, TBM_SETPOS, TRUE, std::clamp(frame, frameRange.start, frameRange.end));
	}

	void PlaybackPanel::SetFrameRange(const int maxFrame) {
		defaultLastFrame = (std::max)(1, maxFrame);
		if (!customTotalFrame)
			totalFrame = defaultLastFrame;
		if (!customFrameRange)
			ApplyFrameRange({0, totalFrame}, false);
		else
			ApplyFrameRange(frameRange, true);
		if (timelineSlider)
			SendMessageW(timelineSlider, TBM_SETPOS, TRUE, frameRange.start);
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

	void PlaybackPanel::Resize(const RECT& clientRect) {
		constexpr int margin = 18;
		constexpr int sliderHeight = 36;
		constexpr int buttonWidth = 72;
		constexpr int buttonHeight = 28;
		constexpr int buttonGap = 8;
		constexpr int frameEditWidth = 66;
		constexpr int separatorWidth = 24;
		constexpr int resetButtonWidth = 64;
		constexpr int totalFrameEditWidth = 72;
		const int width = (std::max)(0, static_cast<int>(clientRect.right - clientRect.left - margin * 2));
		const int sliderY = clientRect.top + 8;
		if (timelineSlider)
			MoveWindow(timelineSlider, clientRect.left + margin, sliderY, width, sliderHeight, TRUE);
		constexpr int buttonTotalWidth = buttonWidth * 3 + buttonGap * 2;
		constexpr int rangeWidth =
			frameEditWidth * 2 + separatorWidth + buttonGap * 2 + resetButtonWidth + totalFrameEditWidth;
		const int buttonX = clientRect.left + margin;
		const int buttonY = sliderY + sliderHeight + 10;
		if (playButton)
			MoveWindow(playButton, buttonX, buttonY, buttonWidth, buttonHeight, TRUE);
		if (pauseButton)
			MoveWindow(pauseButton, buttonX + buttonWidth + buttonGap, buttonY, buttonWidth, buttonHeight, TRUE);
		if (stopButton)
			MoveWindow(stopButton, buttonX + (buttonWidth + buttonGap) * 2, buttonY, buttonWidth, buttonHeight, TRUE);
		const int rangeX = (std::max)(
			buttonX + buttonTotalWidth + buttonGap,
			static_cast<int>(clientRect.right) - margin - rangeWidth);
		if (startFrameEdit)
			MoveWindow(startFrameEdit, rangeX, buttonY, frameEditWidth, buttonHeight, TRUE);
		if (rangeSeparatorText)
			MoveWindow(rangeSeparatorText, rangeX + frameEditWidth, buttonY + 4, separatorWidth, buttonHeight, TRUE);
		if (endFrameEdit)
			MoveWindow(endFrameEdit, rangeX + frameEditWidth + separatorWidth, buttonY, frameEditWidth, buttonHeight, TRUE);
		if (resetRangeButton)
			MoveWindow(resetRangeButton, rangeX + frameEditWidth * 2 + separatorWidth + buttonGap,
				buttonY, resetButtonWidth, buttonHeight, TRUE);
		if (totalFrameEdit)
			MoveWindow(totalFrameEdit,
				rangeX + frameEditWidth * 2 + separatorWidth + buttonGap * 2 + resetButtonWidth,
				buttonY, totalFrameEditWidth, buttonHeight, TRUE);
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
		else if (commandId == controlIds.totalFrameEdit && notificationCode == EN_KILLFOCUS)
			ApplyInputTotalFrame();
		else if (commandId == controlIds.resetRangeButton) {
			totalFrame = defaultLastFrame;
			customTotalFrame = false;
			ApplyFrameRange({0, totalFrame}, false);
			timelineRangeChanged = true;
		}
		else
			return false;
		return true;
	}

	bool PlaybackPanel::HandleScroll(const HWND control, const int scrollCode) {
		if (control != timelineSlider)
			return false;
		seekFrame = std::clamp(
			static_cast<int>(SendMessageW(timelineSlider, TBM_GETPOS, 0, 0)),
			frameRange.start,
			frameRange.end);
		SendMessageW(timelineSlider, TBM_SETPOS, TRUE, seekFrame);
		seekRequested = true;
		seekFinished = scrollCode == TB_ENDTRACK;
		return true;
	}

	void PlaybackPanel::Destroy() {
		if (panelWindow)
			DestroyWindow(panelWindow);
		if (timelineSlider)
			DestroyWindow(timelineSlider);
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
		if (totalFrameEdit)
			DestroyWindow(totalFrameEdit);
		panelWindow = nullptr;
		timelineSlider = nullptr;
		playButton = nullptr;
		pauseButton = nullptr;
		stopButton = nullptr;
		startFrameEdit = nullptr;
		rangeSeparatorText = nullptr;
		endFrameEdit = nullptr;
		resetRangeButton = nullptr;
		totalFrameEdit = nullptr;
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

	bool PlaybackPanel::ConsumeTimelineRangeChange(PlaybackFrameRange& range, int& total) {
		if (!timelineRangeChanged)
			return false;
		range = frameRange;
		total = totalFrame;
		timelineRangeChanged = false;
		return true;
	}
}
