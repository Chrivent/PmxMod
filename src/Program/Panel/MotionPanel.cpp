#include "MotionPanel.h"

#include "../GuiDrawer.h"

#include <algorithm>

namespace Chrivent {
	LRESULT CALLBACK MotionPanel::WindowProc(
		const HWND hwnd,
		const UINT msg,
		const WPARAM wParam,
		const LPARAM lParam) {
		auto* panel = reinterpret_cast<MotionPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (msg == WM_NCCREATE) {
			const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			panel = static_cast<MotionPanel*>(create->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
			panel->timelineWindow = hwnd;
		}
		if (!panel)
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		switch (msg) {
			case WM_SIZE:
				panel->UpdateScrollBars();
				InvalidateRect(hwnd, nullptr, TRUE);
				return 0;
			case WM_VSCROLL:
				panel->ScrollRows(wParam, wParam);
				return 0;
			case WM_HSCROLL:
				panel->ScrollFrames(wParam, wParam);
				return 0;
			case WM_MOUSEWHEEL:
				panel->ScrollRows(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
				return 0;
			case WM_PAINT: {
				PAINTSTRUCT paint{};
				const HDC deviceContext = BeginPaint(hwnd, &paint);
				panel->Paint(deviceContext);
				EndPaint(hwnd, &paint);
				return 0;
			}
			case WM_DESTROY:
				panel->timelineWindow = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void MotionPanel::UpdateScrollBars() const {
		if (!timelineWindow)
			return;
		RECT client{};
		GetClientRect(timelineWindow, &client);
		const int visibleRows = (std::max)(1, static_cast<int>((client.bottom - kHeaderHeight) / kRowHeight));
		const int visibleFrames = (std::max)(1, static_cast<int>((client.right - kLabelWidth) / kFrameWidth));
		SCROLLINFO vertical{};
		vertical.cbSize = sizeof(vertical);
		vertical.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		vertical.nMin = 0;
		vertical.nMax = (std::max)(0, static_cast<int>(rows.size()) - 1);
		vertical.nPage = visibleRows;
		vertical.nPos = firstRow;
		SetScrollInfo(timelineWindow, SB_VERT, &vertical, TRUE);
		SCROLLINFO horizontal{};
		horizontal.cbSize = sizeof(horizontal);
		horizontal.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
		horizontal.nMin = 0;
		horizontal.nMax = static_cast<int>(lastFrame);
		horizontal.nPage = visibleFrames;
		horizontal.nPos = firstFrame;
		SetScrollInfo(timelineWindow, SB_HORZ, &horizontal, TRUE);
	}

	void MotionPanel::Paint(const HDC deviceContext) const {
		RECT client{};
		GetClientRect(timelineWindow, &client);
		GuiDrawer::FillRectColor(deviceContext, client, RGB(26, 29, 35));
		constexpr RECT modelHeader{0, 0, kLabelWidth, kHeaderHeight};
		GuiDrawer::FillRectColor(deviceContext, modelHeader, RGB(57, 61, 70));
		GuiDrawer::DrawTextLine(deviceContext, modelName.empty() ? L"모델을 선택하세요" : modelName,
			{8, 0, kLabelWidth - 4, kHeaderHeight}, RGB(235, 235, 238), DT_LEFT | DT_END_ELLIPSIS);
		const int timelineWidth = client.right - kLabelWidth;
		const int visibleFrames = (std::max)(0, timelineWidth / kFrameWidth + 1);
		for (int offset = 0; offset < visibleFrames; offset++) {
			const int frame = firstFrame + offset;
			const int x = kLabelWidth + offset * kFrameWidth;
			const bool major = frame % 5 == 0;
			GuiDrawer::DrawLine(deviceContext, x, 0, x, client.bottom,
				major ? RGB(65, 77, 92) : RGB(43, 49, 59));
			if (major)
				GuiDrawer::DrawTextLine(deviceContext, std::to_wstring(frame),
					{x + 2, 0, x + kFrameWidth * 5, kHeaderHeight}, RGB(207, 211, 218), DT_LEFT);
		}
		const int visibleRows = (std::max)(0, static_cast<int>((client.bottom - kHeaderHeight) / kRowHeight + 1));
		for (int offset = 0; offset < visibleRows; offset++) {
			const int rowIndex = firstRow + offset;
			if (rowIndex >= static_cast<int>(rows.size()))
				break;
			const int top = kHeaderHeight + offset * kRowHeight;
			const RECT labelRect{0, top, kLabelWidth, top + kRowHeight};
			GuiDrawer::FillRectColor(deviceContext, labelRect,
				offset % 2 == 0 ? RGB(49, 53, 62) : RGB(43, 47, 55));
			GuiDrawer::DrawTextLine(deviceContext, rows[rowIndex].name,
				{8, top, kLabelWidth - 4, top + kRowHeight}, RGB(228, 228, 232), DT_LEFT | DT_END_ELLIPSIS);
			GuiDrawer::DrawLine(deviceContext, 0, top + kRowHeight, client.right, top + kRowHeight, RGB(64, 67, 75));
			for (const uint32_t frame : rows[rowIndex].keyFrames) {
				if (frame < static_cast<uint32_t>(firstFrame))
					continue;
				const int x = kLabelWidth + (static_cast<int>(frame) - firstFrame) * kFrameWidth;
				if (x > client.right)
					break;
				GuiDrawer::DrawDiamond(deviceContext, x, top + kRowHeight / 2, 5, RGB(246, 190, 53));
			}
		}
		GuiDrawer::DrawLine(deviceContext, kLabelWidth, 0, kLabelWidth, client.bottom, RGB(93, 98, 108));
		if (currentFrame >= static_cast<uint32_t>(firstFrame)) {
			const int currentX = kLabelWidth + (static_cast<int>(currentFrame) - firstFrame) * kFrameWidth;
			if (currentX <= client.right)
				GuiDrawer::DrawLine(deviceContext, currentX, 0, currentX, client.bottom, RGB(52, 211, 235));
		}
	}

	void MotionPanel::ScrollRows(const int scrollCode, const int trackPosition) {
		SCROLLINFO info{};
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		GetScrollInfo(timelineWindow, SB_VERT, &info);
		int position = info.nPos;
		switch (scrollCode) {
			case SB_LINEUP: position--; break;
			case SB_LINEDOWN: position++; break;
			case SB_PAGEUP: position -= info.nPage; break;
			case SB_PAGEDOWN: position += info.nPage; break;
			case SB_THUMBPOSITION:
			case SB_THUMBTRACK: position = trackPosition; break;
			default: return;
		}
		firstRow = std::clamp(position, info.nMin, (std::max)(info.nMin, info.nMax - static_cast<int>(info.nPage) + 1));
		SetScrollPos(timelineWindow, SB_VERT, firstRow, TRUE);
		InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::ScrollFrames(const int scrollCode, const int trackPosition) {
		SCROLLINFO info{};
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		GetScrollInfo(timelineWindow, SB_HORZ, &info);
		int position = info.nPos;
		switch (scrollCode) {
			case SB_LINELEFT: position--; break;
			case SB_LINERIGHT: position++; break;
			case SB_PAGELEFT: position -= info.nPage; break;
			case SB_PAGERIGHT: position += info.nPage; break;
			case SB_THUMBPOSITION:
			case SB_THUMBTRACK: position = trackPosition; break;
			default: return;
		}
		firstFrame = std::clamp(position, info.nMin, (std::max)(info.nMin, info.nMax - static_cast<int>(info.nPage) + 1));
		SetScrollPos(timelineWindow, SB_HORZ, firstFrame, TRUE);
		InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::Create(const HWND parent) {
		if (timelineWindow)
			return;
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
		windowClass.lpszClassName = L"PmxModMotionTimeline";
		RegisterClassExW(&windowClass);
		timelineWindow = CreateWindowExW(
			WS_EX_CLIENTEDGE, windowClass.lpszClassName, L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
			0, 0, 0, 0,
			parent, nullptr, instance, this);
		UpdateScrollBars();
	}

	void MotionPanel::Resize(const RECT& clientRect) {
		if (!timelineWindow)
			return;
		constexpr int margin = 8;
		const int x = clientRect.left + margin;
		const int y = clientRect.top + margin;
		const int width = (std::max)(0, static_cast<int>(clientRect.right - clientRect.left) - margin * 2);
		const int height = (std::max)(0, static_cast<int>(clientRect.bottom - clientRect.top) - margin * 2);
		MoveWindow(timelineWindow, x, y, width, height, TRUE);
		UpdateScrollBars();
	}

	void MotionPanel::Destroy() {
		if (timelineWindow)
			DestroyWindow(timelineWindow);
		timelineWindow = nullptr;
		modelName.clear();
		rows.clear();
		lastFrame = 0;
		currentFrame = 0;
		firstRow = 0;
		firstFrame = 0;
	}

	void MotionPanel::SetTimeline(
		std::wstring name,
		std::vector<MotionTimelineRow> timelineRows,
		const uint32_t maxFrame) {
		modelName = std::move(name);
		rows = std::move(timelineRows);
		lastFrame = maxFrame;
		currentFrame = 0;
		firstRow = 0;
		firstFrame = 0;
		UpdateScrollBars();
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, TRUE);
	}

	void MotionPanel::SetCurrentFrame(const uint32_t frame) {
		if (currentFrame == frame)
			return;
		currentFrame = frame;
		if (timelineWindow)
			InvalidateRect(timelineWindow, nullptr, FALSE);
	}
}
