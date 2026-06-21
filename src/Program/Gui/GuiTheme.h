#pragma once

#include <cstddef>
#include <windows.h>

namespace Chrivent {
	class GuiTheme {
	public:
		static constexpr COLORREF backgroundColor = RGB(24, 27, 33);
		static constexpr COLORREF controlColor = RGB(36, 40, 48);
		static constexpr COLORREF borderColor = RGB(92, 101, 114);
		static constexpr COLORREF textColor = RGB(229, 232, 238);

		// 채널 인덱스에 대응하는 보간 곡선 색상을 반환한다.
		static COLORREF GetCurveColor(std::size_t channelIndex);
		// 채널 곡선보다 어두운 키 다이아몬드 색상을 반환한다.
		static COLORREF GetCurveKeyColor(std::size_t channelIndex);
		// 현재 언어의 글리프를 안정적으로 표시하는 UI 폰트를 반환한다.
		static HFONT GetFont();
		// GUI 창 배경에 공통으로 사용할 브러시를 반환한다.
		static HBRUSH GetBackgroundBrush();
		// 창과 모든 자식 컨트롤에 다크 테마와 UI 폰트를 적용한다.
		static void ApplyWindow(HWND window);
		// 단일 Win32 컨트롤에 다크 테마와 UI 폰트를 적용한다.
		static void ApplyControl(HWND control);
		// 표준 컨트롤의 배경색과 글자색을 다크 테마에 맞춰 반환한다.
		static LRESULT HandleControlColor(UINT message, WPARAM wParam);
	};
}
