#pragma once

#include "PanelCommandId.h"
#include "Panel.h"

namespace Chrivent {
	class Sound;

	class SoundPanel final : public Panel {
		static constexpr int kVolumeSliderId = PanelCommandId::soundBase + 1;

		Sound* sound = nullptr;
		HWND panelWindow = nullptr;
		HWND titleText = nullptr;
		HWND volumeSlider = nullptr;
		HWND valueText = nullptr;

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		void UpdateValueText() const;
		void ApplySliderValue() const;
		void CreateContent(HWND parent);

	public:
		SoundPanel() = default;

		void BindSound(Sound& soundRef);
		void Show();
		void Poll() const;
		void Resize(const RECT& clientRect) override;
		bool HandleScroll(HWND control) override;
		void Destroy() override;
	};
}
