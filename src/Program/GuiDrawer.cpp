#include "GuiDrawer.h"

#include <CommCtrl.h>

namespace Chrivent {
	HFONT GuiDrawer::GetTextFont() {
		static const HFONT font = CreateFontW(
			-14, 0, 0, 0, FW_NORMAL,
			FALSE, FALSE, FALSE,
			DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE,
			L"Yu Gothic UI");
		return font;
	}

	void GuiDrawer::FillRectColor(const HDC deviceContext, const RECT& rect, const COLORREF color) {
		const HBRUSH brush = CreateSolidBrush(color);
		FillRect(deviceContext, &rect, brush);
		DeleteObject(brush);
	}

	void GuiDrawer::DrawLine(
		const HDC deviceContext,
		const int x1,
		const int y1,
		const int x2,
		const int y2,
		const COLORREF color) {
		const HPEN pen = CreatePen(PS_SOLID, 1, color);
		const HGDIOBJ previousPen = SelectObject(deviceContext, pen);
		MoveToEx(deviceContext, x1, y1, nullptr);
		LineTo(deviceContext, x2, y2);
		SelectObject(deviceContext, previousPen);
		DeleteObject(pen);
	}

	void GuiDrawer::DrawDiamond(
		const HDC deviceContext,
		const int centerX,
		const int centerY,
		const int radius,
		const COLORREF color) {
		const POINT points[] = {
			{centerX, centerY - radius},
			{centerX + radius, centerY},
			{centerX, centerY + radius},
			{centerX - radius, centerY}
		};
		const HBRUSH brush = CreateSolidBrush(color);
		const HPEN pen = CreatePen(PS_SOLID, 1, color);
		const HGDIOBJ previousBrush = SelectObject(deviceContext, brush);
		const HGDIOBJ previousPen = SelectObject(deviceContext, pen);
		Polygon(deviceContext, points, 4);
		SelectObject(deviceContext, previousPen);
		SelectObject(deviceContext, previousBrush);
		DeleteObject(pen);
		DeleteObject(brush);
	}

	void GuiDrawer::DrawCircle(
		const HDC deviceContext,
		const int centerX,
		const int centerY,
		const int radius,
		const COLORREF color) {
		const HBRUSH brush = CreateSolidBrush(color);
		const HPEN pen = CreatePen(PS_SOLID, 1, color);
		const HGDIOBJ previousBrush = SelectObject(deviceContext, brush);
		const HGDIOBJ previousPen = SelectObject(deviceContext, pen);
		Ellipse(
			deviceContext,
			centerX - radius,
			centerY - radius,
			centerX + radius + 1,
			centerY + radius + 1);
		SelectObject(deviceContext, previousPen);
		SelectObject(deviceContext, previousBrush);
		DeleteObject(pen);
		DeleteObject(brush);
	}

	void GuiDrawer::DrawTriangle(
		const HDC deviceContext,
		const int centerX,
		const int centerY,
		const int radius,
		const bool expanded,
		const COLORREF color) {
		const POINT collapsedPoints[] = {
			{centerX - radius / 2, centerY - radius},
			{centerX - radius / 2, centerY + radius},
			{centerX + radius, centerY}
		};
		const POINT expandedPoints[] = {
			{centerX - radius, centerY - radius / 2},
			{centerX + radius, centerY - radius / 2},
			{centerX, centerY + radius}
		};
		const POINT* points = expanded ? expandedPoints : collapsedPoints;
		const HBRUSH brush = CreateSolidBrush(color);
		const HPEN pen = CreatePen(PS_SOLID, 1, color);
		const HGDIOBJ previousBrush = SelectObject(deviceContext, brush);
		const HGDIOBJ previousPen = SelectObject(deviceContext, pen);
		Polygon(deviceContext, points, 3);
		SelectObject(deviceContext, previousPen);
		SelectObject(deviceContext, previousBrush);
		DeleteObject(pen);
		DeleteObject(brush);
	}

	void GuiDrawer::DrawTextLine(
		const HDC deviceContext,
		const std::wstring& text,
		RECT rect,
		const COLORREF color,
		const UINT format) {
		const HGDIOBJ previousFont = SelectObject(deviceContext, GetTextFont());
		SetBkMode(deviceContext, TRANSPARENT);
		SetTextColor(deviceContext, color);
		DrawTextW(deviceContext, text.c_str(), text.size(), &rect, format | DT_SINGLELINE | DT_VCENTER);
		SelectObject(deviceContext, previousFont);
	}

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
