#pragma once

#include "Panel.h"

namespace Chrivent {
	class PlaybackPanel final : public Panel {
		int timelineSliderId = 0;
		HWND panelWindow = nullptr;
		HWND timelineSlider = nullptr;

		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		void CreateContent(HWND parent);

	public:
		PlaybackPanel() = default;

		void SetTimelineSliderId(int id);
		void Show();
		void Poll() const;
		void Resize(const RECT& clientRect) override;
		void Destroy() override;
	};
}
