#include "Program/Panel/SoundPanel.h"

#include "Program/Gui/GuiDrawer.h"
#include "Program/Gui/GuiTheme.h"
#include "Program/Sound.h"

#include <CommCtrl.h>
#include <cmath>

namespace Chrivent {
	void SoundPanel::UpdateValueText() const {
		if (!valueText || !sound)
			return;
		const int percent = std::lround(sound->GetVolume() * 100.0f);
		const std::wstring text = std::to_wstring(percent) + L"%";
		SetWindowTextW(valueText, text.c_str());
	}

	void SoundPanel::ApplySliderValue() const {
		if (!sound || !volumeSlider)
			return;
		const int sliderValue = static_cast<int>(SendMessageW(volumeSlider, TBM_GETPOS, 0, 0));
		sound->ApplyVolume((100 - sliderValue) / 100.0f);
		UpdateValueText();
	}

	void SoundPanel::CreateContent(const HWND parent) {
		if (volumeSlider)
			return;
		const int initialVolume = sound ? std::lround(sound->GetVolume() * 100.0f) : 0;
		volumeSlider = GuiDrawer::CreateVerticalSlider(parent, volumeSliderId, 0, 100, 100 - initialVolume);
		valueText = CreateWindowExW(
			0, L"STATIC", L"",
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			0, 0, 0, 0,
			parent, nullptr, GetModuleHandleW(nullptr), nullptr);
		GuiTheme::ApplyControl(valueText);
		UpdateValueText();
	}

	void SoundPanel::BindSound(Sound& soundRef) {
		sound = &soundRef;
		if (volumeSlider) {
			const int sliderValue = 100 - std::lround(sound->GetVolume() * 100.0f);
			SendMessageW(volumeSlider, TBM_SETPOS, TRUE, sliderValue);
		}
		UpdateValueText();
	}

	void SoundPanel::Resize(const RECT& clientRect) {
		constexpr int margin = 14;
		constexpr int sliderWidth = 64;
		constexpr int valueHeight = 20;
		constexpr int gap = 4;
		const int width = clientRect.right - clientRect.left;
		const int height = clientRect.bottom - clientRect.top;
		const int contentWidth = std::max(0, width - margin * 2);
		const int controlWidth = std::min(sliderWidth, contentWidth);
		const int sliderHeight = std::max(0, height - margin * 2 - valueHeight - gap);
		const int x = clientRect.left + std::max(margin, (width - controlWidth) / 2);
		const int y = clientRect.top + margin;
		if (volumeSlider)
			MoveWindow(volumeSlider, x, y, controlWidth, sliderHeight, TRUE);
		if (valueText)
			MoveWindow(valueText, x, y + sliderHeight + gap, controlWidth, valueHeight, TRUE);
	}

	bool SoundPanel::HandleScroll(const HWND control, const int) {
		if (control != volumeSlider)
			return false;
		ApplySliderValue();
		return true;
	}

	void SoundPanel::Destroy() {
		if (volumeSlider)
			DestroyWindow(volumeSlider);
		if (valueText)
			DestroyWindow(valueText);
		volumeSlider = nullptr;
		valueText = nullptr;
	}
}
