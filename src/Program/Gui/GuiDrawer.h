#pragma once

#include <string>

#include <windows.h>

namespace Chrivent {
	class GuiDrawer {
		// 한글과 일본어를 함께 표시할 수 있는 UI 폰트를 반환한다.
		static HFONT GetTextFont();

	public:
		// 지정한 영역을 단색 브러시로 채운다.
		static void FillRectColor(HDC deviceContext, const RECT& rect, COLORREF color);
		// 지정한 두 점 사이에 단색 선을 그린다.
		static void DrawLine(HDC deviceContext, int x1, int y1, int x2, int y2, COLORREF color);
		// 지정한 중심과 반지름으로 채워진 마름모를 그린다.
		static void DrawDiamond(HDC deviceContext, int centerX, int centerY, int radius, COLORREF color);
		// 펼침 상태에 따라 오른쪽 또는 아래쪽을 향하는 삼각형을 그린다.
		static void DrawTriangle(HDC deviceContext, int centerX, int centerY, int radius, bool expanded, COLORREF color);
		// 지정한 영역에 한 줄 텍스트를 그린다.
		static void DrawTextLine(HDC deviceContext, const std::wstring& text, RECT rect, COLORREF color, UINT format);
		// 눈금 없는 가로 슬라이더 컨트롤을 생성한다.
		static HWND CreateHorizontalSlider(HWND parent, UINT_PTR controlId, int minValue, int maxValue, int initialValue);
		// 눈금이 있는 세로 슬라이더 컨트롤을 생성한다.
		static HWND CreateVerticalTickSlider(HWND parent, UINT_PTR controlId, int minValue, int maxValue, int initialValue, int tickFrequency);
	};
}
