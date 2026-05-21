#pragma once

#include <windows.h>

namespace Chrivent {
	class PanelWindow {
		HWND window = nullptr;

	public:
		PanelWindow() = default;
		~PanelWindow() = default;

		HWND GetWindow() const { return window; }
	};
}
