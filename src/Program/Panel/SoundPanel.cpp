#include "SoundPanel.h"

#include "../Sound.h"

#include <CommCtrl.h>
#include <cmath>
#include <string>

namespace Chrivent {
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

	void SoundPanel::Create(const HWND parent) {
		parentWindow = parent;
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
			parent, reinterpret_cast<HMENU>(kVolumeSliderId), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(volumeSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
		SendMessageW(volumeSlider, TBM_SETTICFREQ, 10, 0);
		valueText = CreateWindowExW(
			0, L"STATIC", L"",
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			0, 0, 0, 0,
			parent, nullptr, GetModuleHandleW(nullptr), nullptr);
		if (sound)
			SendMessageW(volumeSlider, TBM_SETPOS, TRUE, static_cast<LPARAM>(std::round(sound->GetVolume() * 100.0f)));
		UpdateValueText();
	}

	void SoundPanel::Resize(const RECT& clientRect) {
		constexpr int margin = 14;
		constexpr int titleHeight = 20;
		constexpr int sliderWidth = 64;
		constexpr int sliderHeight = 110;
		constexpr int valueHeight = 20;
		const int right = static_cast<int>(clientRect.right);
		const int x = (std::max)(margin, right - margin - sliderWidth);
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
		titleText = nullptr;
		volumeSlider = nullptr;
		valueText = nullptr;
		parentWindow = nullptr;
	}
}
