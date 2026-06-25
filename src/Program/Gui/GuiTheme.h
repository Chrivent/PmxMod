#pragma once

#include <cstddef>
#include <windows.h>

namespace Chrivent {
	class GuiTheme {
	public:
		static constexpr COLORREF backgroundColor = RGB(24, 27, 33);
		static constexpr COLORREF controlColor = RGB(36, 40, 48);
		static constexpr COLORREF disabledControlColor = RGB(31, 34, 41);
		static constexpr COLORREF borderColor = RGB(92, 101, 114);
		static constexpr COLORREF textColor = RGB(229, 232, 238);
		static constexpr COLORREF disabledTextColor = RGB(133, 141, 154);

		static COLORREF GetSelectedCurveKeyColor() { return RGB(246, 190, 53); }
		// 채널 인덱스에 대응하는 보간 곡선 색상을 선택한다.
		static COLORREF ResolveCurveColor(std::size_t channelIndex);
		// 현재 언어에 대응하는 UI 폰트를 선택한다.
		static HFONT ResolveFont();
		// GUI 배경에 공통으로 사용할 브러시를 반환한다.
		static HBRUSH ResolveBackgroundBrush();
		// 창과 모든 자식 컨트롤에 다크 테마와 UI 폰트를 적용한다.
		static void ApplyWindow(HWND window);
		// 단일 Win32 컨트롤에 다크 테마와 UI 폰트를 적용한다.
		static void ApplyControl(HWND control);
		// 표준 컨트롤의 배경색과 글자색을 다크 테마에 맞춰 반환한다.
		static LRESULT HandleControlColor(UINT message, WPARAM wParam);
	};
}
