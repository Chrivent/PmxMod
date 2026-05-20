#pragma once

#include <windows.h>

namespace Chrivent {
	class GuiDrawer {
	public:
		// 눈금 없는 가로 슬라이더 컨트롤을 생성한다.
		static HWND CreateHorizontalSlider(HWND parent, int controlId, int minValue, int maxValue, int initialValue);
		// 눈금이 있는 세로 슬라이더 컨트롤을 생성한다.
		static HWND CreateVerticalTickSlider(HWND parent, int controlId, int minValue, int maxValue, int initialValue, int tickFrequency);
	};
}
