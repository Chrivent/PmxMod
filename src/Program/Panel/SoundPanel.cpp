#include "SoundPanel.h"

#include "../Sound.h"

#include <CommCtrl.h>
#include <cmath>
#include <string>

namespace Chrivent {
	LRESULT CALLBACK SoundPanel::WindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		auto* panel = reinterpret_cast<SoundPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (msg == WM_NCCREATE) {
			const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			panel = static_cast<SoundPanel*>(create->lpCreateParams);
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
			case WM_HSCROLL:
			case WM_VSCROLL:
				if (panel->HandleScroll(reinterpret_cast<HWND>(lParam)))
					return 0;
				break;
			case WM_CLOSE:
				ShowWindow(hwnd, SW_HIDE);
				return 0;
			case WM_DESTROY:
				panel->panelWindow = nullptr;
				panel->titleText = nullptr;
				panel->volumeSlider = nullptr;
				panel->valueText = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void SoundPanel::UpdateValueText() const {
		if (!valueText || !sound)
			return;
		const int percent = static_cast<int>(std::round(sound->GetVolume() * 100.0f));
		const std::wstring text = std::to_wstring(percent) + L"%";
		SetWindowTextW(valueText, text.c_str());
	}

	void SoundPanel::ApplySliderValue() const {
		if (!sound || !volumeSlider)
			return;
		const auto sliderValue = static_cast<int>(SendMessageW(volumeSlider, TBM_GETPOS, 0, 0));
		sound->SetVolume(static_cast<float>(sliderValue) / 100.0f);
		UpdateValueText();
	}

	void SoundPanel::BindSound(Sound& soundRef) {
		sound = &soundRef;
		if (volumeSlider)
			SendMessageW(volumeSlider, TBM_SETPOS, TRUE, std::round(sound->GetVolume() * 100.0f));
		UpdateValueText();
	}

	void SoundPanel::Show() {
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
		wc.lpszClassName = L"PmxModSoundPanel";
		RegisterClassExW(&wc);
		panelWindow = CreateWindowExW(
			0, L"PmxModSoundPanel", L"Sound",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 180, 260,
			nullptr, nullptr, instance, this);
		if (!panelWindow)
			return;
		ShowWindow(panelWindow, SW_SHOWNORMAL);
		UpdateWindow(panelWindow);
	}

	void SoundPanel::Poll() const {
		if (!panelWindow)
			return;
		MSG msg{};
		while (PeekMessageW(&msg, panelWindow, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	void SoundPanel::CreateContent(const HWND parent) {
		INITCOMMONCONTROLSEX init;
		init.dwSize = sizeof(init);
		init.dwICC = ICC_BAR_CLASSES;
		InitCommonControlsEx(&init);
		titleText = CreateWindowExW(
			0, L"STATIC", L"Sound",
			WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0,
			parent, nullptr, GetModuleHandleW(nullptr), nullptr);
		volumeSlider = CreateWindowExW(
			0, TRACKBAR_CLASSW, L"",
			WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_AUTOTICKS | TBS_BOTH,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kVolumeSliderId)), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(volumeSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
		SendMessageW(volumeSlider, TBM_SETTICFREQ, 10, 0);
		valueText = CreateWindowExW(
			0, L"STATIC", L"",
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			0, 0, 0, 0,
			parent, nullptr, GetModuleHandleW(nullptr), nullptr);
		if (sound)
			SendMessageW(volumeSlider, TBM_SETPOS, TRUE, std::round(sound->GetVolume() * 100.0f));
		UpdateValueText();
	}

	void SoundPanel::Resize(const RECT& clientRect) {
		constexpr int margin = 14;
		constexpr int titleHeight = 20;
		constexpr int sliderWidth = 64;
		constexpr int sliderHeight = 150;
		constexpr int valueHeight = 20;
		const int right = static_cast<int>(clientRect.right);
		const int x = (std::max)(margin, (right - sliderWidth) / 2);
		constexpr int y = 14;
		if (titleText)
			MoveWindow(titleText, x, y, sliderWidth, titleHeight, TRUE);
		if (volumeSlider)
			MoveWindow(volumeSlider, x, y + titleHeight + 4, sliderWidth, sliderHeight, TRUE);
		if (valueText)
			MoveWindow(valueText, x, y + titleHeight + 4 + sliderHeight + 4, sliderWidth, valueHeight, TRUE);
	}

	bool SoundPanel::HandleScroll(const HWND control) {
		if (control != volumeSlider)
			return false;
		ApplySliderValue();
		return true;
	}

	void SoundPanel::Destroy() {
		if (panelWindow)
			DestroyWindow(panelWindow);
		panelWindow = nullptr;
		titleText = nullptr;
		volumeSlider = nullptr;
		valueText = nullptr;
	}
}
