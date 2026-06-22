#include "GuiDrawer.h"

#include "GuiTheme.h"

#include <CommCtrl.h>
#include <algorithm>
#include <cmath>

namespace Chrivent {
	HFONT GuiDrawer::GetTextFont() {
		return GuiTheme::GetFont();
	}

	void GuiDrawer::FillRectColor(const HDC deviceContext, const RECT& rect, const COLORREF color) {
		const HBRUSH brush = CreateSolidBrush(color);
		FillRect(deviceContext, &rect, brush);
		DeleteObject(brush);
	}

	void GuiDrawer::DrawLine(const HDC deviceContext, const int x1, const int y1, const int x2, const int y2, const COLORREF color) {
		const HPEN pen = CreatePen(PS_SOLID, 1, color);
		const HGDIOBJ previousPen = SelectObject(deviceContext, pen);
		MoveToEx(deviceContext, x1, y1, nullptr);
		LineTo(deviceContext, x2, y2);
		SelectObject(deviceContext, previousPen);
		DeleteObject(pen);
	}

	void GuiDrawer::DrawDiamond(const HDC deviceContext, const int centerX, const int centerY, const int radius, const COLORREF color) {
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

	void GuiDrawer::DrawWaveform(const HDC deviceContext, const RECT& rect,
		const std::vector<float>& minimums, const std::vector<float>& maximums,
		const int samplesPerFrame, const int firstFrame, const int frameWidth, const COLORREF color) {
		if (minimums.empty() || minimums.size() != maximums.size() ||
			samplesPerFrame <= 0 || frameWidth <= 0 ||
			rect.right <= rect.left || rect.bottom <= rect.top)
			return;
		const int centerY = rect.top + (rect.bottom - rect.top) / 2;
		int amplitudeHeight = (rect.bottom - rect.top) / 2 - 8;
		if (amplitudeHeight < 1)
			amplitudeHeight = 1;
		const HPEN pen = CreatePen(PS_SOLID, 1, color);
		const HGDIOBJ previousPen = SelectObject(deviceContext, pen);
		for (int x = rect.left; x < rect.right; x++) {
			const size_t pixelOffset = x - rect.left;
			const size_t firstSample = (firstFrame * frameWidth + pixelOffset) * samplesPerFrame / frameWidth;
			size_t lastSample = (firstFrame * frameWidth + pixelOffset + 1) * samplesPerFrame / frameWidth;
			if (lastSample <= firstSample)
				lastSample = firstSample + 1;
			if (firstSample >= minimums.size())
				continue;
			lastSample = (std::min)(lastSample, minimums.size());
			float minimum = 1.0f;
			float maximum = -1.0f;
			for (size_t sample = firstSample; sample < lastSample; sample++) {
				minimum = (std::min)(minimum, minimums[sample]);
				maximum = (std::max)(maximum, maximums[sample]);
			}
			const int top = centerY - std::lround(maximum * amplitudeHeight);
			const int bottom = centerY - std::lround(minimum * amplitudeHeight);
			MoveToEx(deviceContext, x, top, nullptr);
			LineTo(deviceContext, x, bottom + 1);
		}
		SelectObject(deviceContext, previousPen);
		DeleteObject(pen);
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

	void GuiDrawer::DrawTextLine(const HDC deviceContext, const std::wstring& text, RECT rect, const COLORREF color, const UINT format) {
		const HGDIOBJ previousFont = SelectObject(deviceContext, GetTextFont());
		SetBkMode(deviceContext, TRANSPARENT);
		SetTextColor(deviceContext, color);
		DrawTextW(deviceContext, text.c_str(), text.size(), &rect, format | DT_SINGLELINE | DT_VCENTER);
		SelectObject(deviceContext, previousFont);
	}

	HWND GuiDrawer::CreateHorizontalSlider(const HWND parent, const UINT_PTR controlId, const int minValue, const int maxValue, const int initialValue) {
		INITCOMMONCONTROLSEX init;
		init.dwSize = sizeof(init);
		init.dwICC = ICC_BAR_CLASSES;
		InitCommonControlsEx(&init);
		const HWND slider = CreateWindowExW(
			0, TRACKBAR_CLASSW, L"",
			WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(controlId), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(minValue, maxValue));
		SendMessageW(slider, TBM_SETPOS, TRUE, initialValue);
		GuiTheme::ApplyControl(slider);
		return slider;
	}

	HWND GuiDrawer::CreateVerticalTickSlider(const HWND parent, const UINT_PTR controlId, const int minValue, const int maxValue, const int initialValue, const int tickFrequency) {
		INITCOMMONCONTROLSEX init;
		init.dwSize = sizeof(init);
		init.dwICC = ICC_BAR_CLASSES;
		InitCommonControlsEx(&init);
		const HWND slider = CreateWindowExW(
			0, TRACKBAR_CLASSW, L"",
			WS_CHILD | WS_VISIBLE | TBS_VERT | TBS_AUTOTICKS | TBS_BOTH,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(controlId), GetModuleHandleW(nullptr), nullptr);
		SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(minValue, maxValue));
		SendMessageW(slider, TBM_SETTICFREQ, tickFrequency, 0);
		SendMessageW(slider, TBM_SETPOS, TRUE, initialValue);
		GuiTheme::ApplyControl(slider);
		return slider;
	}
}
