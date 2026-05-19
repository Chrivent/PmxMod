#pragma once

#include "ToolPanel.h"

namespace Chrivent {
	class Sound;

	class SoundPanel final : public ToolPanel {
		static constexpr int kVolumeSliderId = 1;

		Sound* sound = nullptr;
		HWND parentWindow = nullptr;
		HWND titleText = nullptr;
		HWND volumeSlider = nullptr;
		HWND valueText = nullptr;

		void UpdateValueText() const;
		void ApplySliderValue() const;

	public:
		SoundPanel() = default;

		void BindSound(Sound& soundRef);
		void Create(HWND parent) override;
		void Resize(const RECT& clientRect) override;
		bool HandleScroll(HWND control) override;
		void Destroy() override;
	};
}
