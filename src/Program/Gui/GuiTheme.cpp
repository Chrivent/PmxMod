#include "Program/Gui/GuiTheme.h"

#include "Program/Language.h"

#include <CommCtrl.h>
#include <dwmapi.h>
#include <iostream>
#include <uxtheme.h>

namespace Chrivent {
	COLORREF GuiTheme::ResolveCurveColor(const std::size_t channelIndex) {
		constexpr COLORREF colors[] = {
			RGB(255, 105, 105),
			RGB(120, 225, 130),
			RGB(105, 165, 255),
			RGB(214, 145, 255),
			RGB(255, 135, 185),
			RGB(95, 220, 220)
		};
		return colors[channelIndex % std::size(colors)];
	}

	HFONT GuiTheme::ResolveFont() {
		static const HFONT englishFont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
		static const HFONT koreanFont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
		static const HFONT japaneseFont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
		static const HFONT chineseFont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GB2312_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
		switch (Language::GetCurrent()) {
			case LanguageType::Korean:
				return koreanFont;
			case LanguageType::Japanese:
				return japaneseFont;
			case LanguageType::Chinese:
				return chineseFont;
			case LanguageType::English:
			default:
				return englishFont;
		}
	}

	HBRUSH GuiTheme::GetBackgroundBrush() {
		static const HBRUSH brush = CreateSolidBrush(backgroundColor);
		return brush;
	}

	void GuiTheme::ApplyWindow(const HWND window) {
		if (!window)
			return;
		constexpr BOOL enabled = TRUE;
		constexpr DWORD immersiveDarkModeAttribute = 20;
		const HRESULT result = DwmSetWindowAttribute(window, immersiveDarkModeAttribute, &enabled, sizeof(enabled));
		if (FAILED(result))
			std::cerr << "어두운 윈도우 프레임을 적용하지 못했습니다: 0x"
				<< std::hex << static_cast<unsigned long>(result) << std::dec << '\n';
		ApplyControl(window);
		EnumChildWindows(window, [](const HWND child, const LPARAM) {
			ApplyControl(child);
			return TRUE;
		}, 0);
		RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
	}

	void GuiTheme::ApplyControl(const HWND control) {
		if (!control)
			return;
		SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(ResolveFont()), TRUE);
		wchar_t className[32]{};
		GetClassNameW(control, className, 32);
		const HRESULT result = lstrcmpW(className, TRACKBAR_CLASSW) == 0
			? SetWindowTheme(control, L"", L"")
			: SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
		if (FAILED(result))
			std::cerr << "Win32 control theme을 적용하지 못했습니다: 0x"
				<< std::hex << static_cast<unsigned long>(result) << std::dec << '\n';
	}

	LRESULT GuiTheme::HandleControlColor(const UINT message, const WPARAM wParam) {
		const auto deviceContext = reinterpret_cast<HDC>(wParam);
		SetTextColor(deviceContext, textColor);
		if (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX) {
			static const HBRUSH controlBrush = CreateSolidBrush(controlColor);
			SetBkColor(deviceContext, controlColor);
			return reinterpret_cast<LRESULT>(controlBrush);
		}
		SetBkColor(deviceContext, backgroundColor);
		SetBkMode(deviceContext, TRANSPARENT);
		return reinterpret_cast<LRESULT>(GetBackgroundBrush());
	}
}
