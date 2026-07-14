#pragma once

#include <string>
#include <vector>

#include <windows.h>

namespace Chrivent {
	// Win32 GDI를 사용해 패널의 공통 컨트롤과 장식을 그린다.
	class GuiDrawer {
		static HFONT GetTextFont();

	public:
		// 지정한 영역을 단색 브러시로 채운다.
		static void FillRectColor(HDC deviceContext, const RECT& rect, COLORREF color);
		// 지정한 두 점 사이에 단색 선을 그린다.
		static void DrawLine(HDC deviceContext, int x1, int y1, int x2, int y2, COLORREF color);
		// 지정한 중심과 반지름으로 채워진 마름모를 그린다.
		static void DrawDiamond(HDC deviceContext, int centerX, int centerY, int radius, COLORREF color);
		// 고밀도 최소·최대 진폭 데이터를 화면 픽셀 범위에 맞춰 파형으로 그린다.
		static void DrawWaveform(HDC deviceContext, const RECT& rect,
			const std::vector<float>& minimums, const std::vector<float>& maximums,
			int samplesPerFrame, int firstFrame, int frameWidth, COLORREF color);
		// 펼침 상태에 따라 오른쪽 또는 아래쪽을 향하는 삼각형을 그린다.
		static void DrawTriangle(HDC deviceContext, int centerX, int centerY, int radius, bool expanded, COLORREF color);
		// 지정한 영역에 한 줄 텍스트를 그린다.
		static void DrawTextLine(HDC deviceContext, const std::wstring& text, RECT rect, COLORREF color, UINT format);
		// 눈금 없는 가로 슬라이더 컨트롤을 생성한다.
		static HWND CreateHorizontalSlider(HWND parent, UINT_PTR controlId, int minValue, int maxValue, int initialValue);
		// 눈금이 있는 세로 슬라이더 컨트롤을 생성한다.
		// 눈금 없는 세로 트랙바를 생성한다.
		static HWND CreateVerticalSlider(HWND parent, UINT_PTR controlId, int minValue, int maxValue, int initialValue);
	};
}
