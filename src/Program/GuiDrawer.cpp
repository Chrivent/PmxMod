#include "GuiDrawer.h"

#include <CommCtrl.h>

namespace Chrivent {
	HWND GuiDrawer::CreateHorizontalSlider(const HWND parent, const int controlId, const int minValue, const int maxValue, const int initialValue) {
		INITCOMMONCONTROLSEX init;
		init.dwSize = sizeof(init);
		init.dwICC = ICC_BAR_CLASSES;
		InitCommonControlsEx(&init);
		const HWND slider = CreateWindowExW(
			0, TRACKBAR_CLASSW, L"",
			WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(minValue, maxValue));
		SendMessageW(slider, TBM_SETPOS, TRUE, initialValue);
		return slider;
	}

	HWND GuiDrawer::CreateVerticalTickSlider(
		const HWND parent,
		const int controlId,
		const int minValue,
		const int maxValue,
		const int initialValue,
		const int tickFrequency) {
		INITCOMMONCONTROLSEX init;
		init.dwSize = sizeof(init);
		init.dwICC = ICC_BAR_CLASSES;
		InitCommonControlsEx(&init);
		const HWND slider = CreateWindowExW(
			0, TRACKBAR_CLASSW, L"",
			WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_AUTOTICKS | TBS_BOTH,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(minValue, maxValue));
		SendMessageW(slider, TBM_SETTICFREQ, tickFrequency, 0);
		SendMessageW(slider, TBM_SETPOS, TRUE, initialValue);
		return slider;
	}
}
