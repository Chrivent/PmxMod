#include "InterpolationCurvePanel.h"

#include "../Gui/GuiBackBuffer.h"
#include "../Gui/GuiDrawer.h"
#include "../Gui/GuiTheme.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <windowsx.h>

namespace Chrivent {
	LRESULT CALLBACK InterpolationCurvePanel::WindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		auto* panel = reinterpret_cast<InterpolationCurvePanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (msg == WM_NCCREATE) {
			const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			panel = static_cast<InterpolationCurvePanel*>(create->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
			panel->graphWindow = hwnd;
		}
		if (!panel)
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		switch (msg) {
			case WM_LBUTTONDOWN:
				panel->SelectControlPoint(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				return 0;
			case WM_ERASEBKGND:
				return 1;
			case WM_PAINT: {
				PAINTSTRUCT paint{};
				const HDC targetDc = BeginPaint(hwnd, &paint);
				RECT client{};
				GetClientRect(hwnd, &client);
				const GuiBackBuffer backBuffer(targetDc, client);
				if (const HDC memoryDc = backBuffer.GetDc()) {
					panel->Paint(memoryDc);
					backBuffer.Present();
				} else
					panel->Paint(targetDc);
				EndPaint(hwnd, &paint);
				return 0;
			}
			case WM_DESTROY:
				panel->graphWindow = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void InterpolationCurvePanel::SelectControlPoint(const int x, const int y) {
		const int previousChannel = selectedChannel;
		const int previousCurve = selectedCurve;
		const int previousControlPoint = selectedControlPoint;
		selectedChannel = -1;
		selectedCurve = -1;
		selectedControlPoint = -1;
		if (selection.selectedKeyCount == 1 && graphWindow) {
			RECT client{};
			GetClientRect(graphWindow, &client);
			const size_t channelCount = selection.channels.size();
			const int width = client.right - client.left;
			const int height = client.bottom - client.top;
			const int channelHeight = channelCount == 0 ? 0 : height / channelCount;
			for (size_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
				constexpr int labelHeight = 18;
				constexpr int padding = 12;
				const auto& [name, curves] = selection.channels[channelIndex];
				const int areaTop = channelIndex * channelHeight;
				const int areaBottom = channelIndex + 1 == channelCount ? height : areaTop + channelHeight;
				const int graphSize = (std::max)(0,
					(std::min)(width - padding * 2, areaBottom - areaTop - labelHeight - padding * 2));
				if (graphSize <= 0)
					continue;
				const int left = (width - graphSize) / 2;
				const int top = areaTop + labelHeight + (areaBottom - areaTop - labelHeight - graphSize) / 2;
				const int bottom = top + graphSize;
				for (size_t curveIndex = 0; curveIndex < curves.size(); curveIndex++) {
					const auto& [p1, p2] = curves[curveIndex];
					const POINT points[] = {
						{
							left + std::lround(std::clamp(p1.x, 0.0f, 1.0f) * graphSize),
							bottom - std::lround(std::clamp(p1.y, 0.0f, 1.0f) * graphSize)
						},
						{
							left + std::lround(std::clamp(p2.x, 0.0f, 1.0f) * graphSize),
							bottom - std::lround(std::clamp(p2.y, 0.0f, 1.0f) * graphSize)
						}
					};
					for (int pointIndex = 0; pointIndex < 2; pointIndex++) {
						if (std::abs(x - points[pointIndex].x) + std::abs(y - points[pointIndex].y) > 7)
							continue;
						selectedChannel = channelIndex;
						selectedCurve = curveIndex;
						selectedControlPoint = pointIndex;
						break;
					}
					if (selectedControlPoint >= 0)
						break;
				}
				if (selectedControlPoint >= 0)
					break;
			}
		}
		if (selectedChannel != previousChannel || selectedCurve != previousCurve ||
			selectedControlPoint != previousControlPoint)
			InvalidateRect(graphWindow, nullptr, FALSE);
	}

	void InterpolationCurvePanel::Paint(const HDC deviceContext) const {
		RECT client{};
		GetClientRect(graphWindow, &client);
		GuiDrawer::FillRectColor(deviceContext, client, RGB(26, 29, 35));
		const size_t channelCount = selection.channels.size();
		if (channelCount == 0)
			return;
		const int width = client.right - client.left;
		const int height = client.bottom - client.top;
		const int channelHeight = height / channelCount;
		const bool showControlPoints = selection.selectedKeyCount == 1;
		for (size_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
			constexpr int labelHeight = 18;
			constexpr int padding = 12;
			const auto& [name, curves] = selection.channels[channelIndex];
			if (curves.empty())
				continue;
			const int areaTop = channelIndex * channelHeight;
			const int areaBottom = channelIndex + 1 == channelCount ? height : areaTop + channelHeight;
			const int graphSize = (std::max)(0, (std::min)(width - padding * 2, areaBottom - areaTop - labelHeight - padding * 2));
			if (graphSize <= 0)
				continue;
			const int left = (width - graphSize) / 2;
			const int top = areaTop + labelHeight + (areaBottom - areaTop - labelHeight - graphSize) / 2;
			const int right = left + graphSize;
			const int bottom = top + graphSize;
			GuiDrawer::DrawTextLine(deviceContext, name,
				{ padding, areaTop, width - padding, areaTop + labelHeight },
				RGB(220, 224, 230),
				DT_LEFT | DT_END_ELLIPSIS);
			for (int index = 0; index <= 10; index++) {
				const int x = left + graphSize * index / 10;
				const int y = top + graphSize * index / 10;
				const COLORREF color = index == 0 || index == 10
					? RGB(105, 110, 120)
					: RGB(48, 54, 64);
				GuiDrawer::DrawLine(deviceContext, x, top, x, bottom, color);
				GuiDrawer::DrawLine(deviceContext, left, y, right, y, color);
			}
			for (size_t curveIndex = 0; curveIndex < curves.size(); curveIndex++) {
				const auto& [p1, p2] = curves[curveIndex];
				const auto ToPoint = [&](const glm::vec2 point) {
					return POINT{
						left + std::lround(std::clamp(point.x, 0.0f, 1.0f) * graphSize),
						bottom - std::lround(std::clamp(point.y, 0.0f, 1.0f) * graphSize)
					};
				};
				const POINT controlPoints[] = {
					{ left, bottom },
					ToPoint(p1),
					ToPoint(p2),
					{ right, top }
				};
				if (showControlPoints) {
					GuiDrawer::DrawLine(
						deviceContext,
						controlPoints[0].x,
						controlPoints[0].y,
						controlPoints[1].x,
						controlPoints[1].y,
						RGB(115, 120, 130));
					GuiDrawer::DrawLine(
						deviceContext,
						controlPoints[2].x,
						controlPoints[2].y,
						controlPoints[3].x,
						controlPoints[3].y,
						RGB(115, 120, 130));
				}
				const HPEN curvePen = CreatePen(PS_SOLID, 2, GuiTheme::GetCurveColor(channelIndex));
				const HGDIOBJ previousPen = SelectObject(deviceContext, curvePen);
				PolyBezier(deviceContext, controlPoints, 4);
				SelectObject(deviceContext, previousPen);
				DeleteObject(curvePen);
				if (showControlPoints) {
					for (int pointIndex = 0; pointIndex < 2; pointIndex++) {
						const bool selected = selectedChannel == channelIndex
							&& selectedCurve == curveIndex
							&& selectedControlPoint == pointIndex;
						const COLORREF color = selected
							? GuiTheme::GetSelectedCurveKeyColor()
							: RGB(174, 179, 188);
						GuiDrawer::DrawDiamond(
							deviceContext, controlPoints[pointIndex + 1].x, controlPoints[pointIndex + 1].y, 5, color);
					}
				}
			}
		}
	}

	void InterpolationCurvePanel::Create(const HWND parent) {
		if (graphWindow)
			return;
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		windowClass.lpszClassName = L"PmxModInterpolationCurve";
		RegisterClassExW(&windowClass);
		graphWindow = CreateWindowExW(
			WS_EX_CLIENTEDGE,
			windowClass.lpszClassName,
			L"",
			WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0,
			parent,
			nullptr,
			instance,
			this);
	}

	void InterpolationCurvePanel::Resize(const RECT& clientRect) {
		if (!graphWindow)
			return;
		constexpr int margin = 12;
		const int width = (std::max)(0, static_cast<int>(clientRect.right - clientRect.left) - margin * 2);
		const int height = (std::max)(0, static_cast<int>(clientRect.bottom - clientRect.top) - margin * 2);
		MoveWindow(graphWindow, clientRect.left + margin, clientRect.top + margin, width, height, TRUE);
	}

	void InterpolationCurvePanel::Destroy() {
		if (graphWindow)
			DestroyWindow(graphWindow);
		graphWindow = nullptr;
		selection = {};
		selectedChannel = -1;
		selectedCurve = -1;
		selectedControlPoint = -1;
	}

	void InterpolationCurvePanel::SetSelection(InterpolationSelection interpolationSelection) {
		selection = std::move(interpolationSelection);
		selectedChannel = -1;
		selectedCurve = -1;
		selectedControlPoint = -1;
		if (graphWindow)
			InvalidateRect(graphWindow, nullptr, TRUE);
	}
}
