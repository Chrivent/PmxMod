#include "GuiTheme.h"

#include "Language.h"

#include <CommCtrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

namespace Chrivent {
	HFONT GuiTheme::GetFont() {
		static const HFONT englishFont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, L"Tahoma");
		static const HFONT koreanFont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, L"Malgun Gothic");
		static const HFONT japaneseFont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, L"Yu Gothic UI");
		static const HFONT chineseFont = CreateFontW(
			-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GB2312_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
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

	void GuiTheme::ApplyControl(const HWND control) {
		if (!control)
			return;
		SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetFont()), TRUE);
		wchar_t className[32]{};
		GetClassNameW(control, className, 32);
		if (lstrcmpW(className, TRACKBAR_CLASSW) == 0)
			SetWindowTheme(control, L"", L"");
		else
			SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
	}

	void GuiTheme::ApplyWindow(const HWND window) {
		if (!window)
			return;
		constexpr BOOL enabled = TRUE;
		constexpr DWORD immersiveDarkModeAttribute = 20;
		DwmSetWindowAttribute(window, immersiveDarkModeAttribute, &enabled, sizeof(enabled));
		ApplyControl(window);
		EnumChildWindows(window, [](const HWND child, const LPARAM) {
			ApplyControl(child);
			return TRUE;
		}, 0);
		RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE);
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
