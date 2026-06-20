#include "InterpolationCurvePanel.h"

#include "../GuiBackBuffer.h"
#include "../GuiDrawer.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Chrivent {
	LRESULT CALLBACK InterpolationCurvePanel::WindowProc(
		const HWND hwnd,
		const UINT msg,
		const WPARAM wParam,
		const LPARAM lParam) {
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

	void InterpolationCurvePanel::Paint(const HDC deviceContext) const {
		RECT client{};
		GetClientRect(graphWindow, &client);
		GuiDrawer::FillRectColor(deviceContext, client, RGB(26, 29, 35));
		const size_t channelCount = selection.channels.size();
		if (channelCount == 0)
			return;
		constexpr COLORREF curveColors[] = {
			RGB(100, 220, 255),
			RGB(246, 190, 53),
			RGB(255, 125, 176),
			RGB(133, 225, 133),
			RGB(188, 150, 255),
			RGB(255, 160, 105)
		};
		const int width = client.right - client.left;
		const int height = client.bottom - client.top;
		const int channelHeight = height / static_cast<int>(channelCount);
		const bool showControlPoints = selection.selectedKeyCount == 1;
		for (size_t channelIndex = 0; channelIndex < channelCount; channelIndex++) {
			constexpr int labelHeight = 18;
			constexpr int padding = 12;
			const auto& channel = selection.channels[channelIndex];
			if (channel.curves.empty())
				continue;
			const int areaTop = static_cast<int>(channelIndex) * channelHeight;
			const int areaBottom = channelIndex + 1 == channelCount
				? height
				: areaTop + channelHeight;
			const int graphSize = (std::max)(0, (std::min)(width - padding * 2, areaBottom - areaTop - labelHeight - padding * 2));
			if (graphSize <= 0)
				continue;
			const int left = (width - graphSize) / 2;
			const int top = areaTop + labelHeight + (areaBottom - areaTop - labelHeight - graphSize) / 2;
			const int right = left + graphSize;
			const int bottom = top + graphSize;
			GuiDrawer::DrawTextLine(
				deviceContext,
				channel.name,
				{padding, areaTop, width - padding, areaTop + labelHeight},
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
			for (size_t curveIndex = 0; curveIndex < channel.curves.size(); curveIndex++) {
				const auto& curve = channel.curves[curveIndex];
				const auto ToPoint = [&](const glm::vec2 point) {
					return POINT{
						left + std::lround(std::clamp(point.x, 0.0f, 1.0f) * graphSize),
						bottom - std::lround(std::clamp(point.y, 0.0f, 1.0f) * graphSize)
					};
				};
				const POINT controlPoints[] = {
					{left, bottom},
					ToPoint(curve.p1),
					ToPoint(curve.p2),
					{right, top}
				};
				if (showControlPoints) {
					GuiDrawer::DrawLine(
						deviceContext,
						controlPoints[0].x,
						controlPoints[0].y,
						controlPoints[1].x,
						controlPoints[1].y,
						RGB(100, 110, 124));
					GuiDrawer::DrawLine(
						deviceContext,
						controlPoints[2].x,
						controlPoints[2].y,
						controlPoints[3].x,
						controlPoints[3].y,
						RGB(100, 110, 124));
				}
				const HPEN curvePen = CreatePen(PS_SOLID, 2, curveColors[curveIndex % std::size(curveColors)]);
				const HGDIOBJ previousPen = SelectObject(deviceContext, curvePen);
				PolyBezier(deviceContext, controlPoints, 4);
				SelectObject(deviceContext, previousPen);
				DeleteObject(curvePen);
				if (showControlPoints) {
					GuiDrawer::DrawDiamond(deviceContext, controlPoints[1].x, controlPoints[1].y, 5, RGB(246, 190, 53));
					GuiDrawer::DrawDiamond(deviceContext, controlPoints[2].x, controlPoints[2].y, 5, RGB(246, 190, 53));
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
	}

	void InterpolationCurvePanel::SetSelection(InterpolationSelection interpolationSelection) {
		selection = std::move(interpolationSelection);
		if (graphWindow)
			InvalidateRect(graphWindow, nullptr, TRUE);
	}
}
