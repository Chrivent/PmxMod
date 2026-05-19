#include "PlaybackPanel.h"

#include "../Manager/GuiManager.h"
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
			case WM_CLOSE:
				ShowWindow(hwnd, SW_HIDE);
				return 0;
			case WM_DESTROY:
				panel->panelWindow = nullptr;
				panel->timelineSlider = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void PlaybackPanel::SetTimelineSliderId(const int id) {
		timelineSliderId = id;
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
			CW_USEDEFAULT, CW_USEDEFAULT, 420, 120,
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
		timelineSlider = GuiManager::CreateHorizontalSlider(parent, timelineSliderId, 0, 1000, 0);
	}

	void PlaybackPanel::Resize(const RECT& clientRect) {
		constexpr int margin = 18;
		constexpr int sliderHeight = 36;
		const int width = (std::max)(0, static_cast<int>(clientRect.right) - margin * 2);
		const int y = (std::max)(margin, (static_cast<int>(clientRect.bottom) - sliderHeight) / 2);
		if (timelineSlider)
			MoveWindow(timelineSlider, margin, y, width, sliderHeight, TRUE);
	}

	void PlaybackPanel::Destroy() {
		if (panelWindow)
			DestroyWindow(panelWindow);
		panelWindow = nullptr;
		timelineSlider = nullptr;
	}
}
