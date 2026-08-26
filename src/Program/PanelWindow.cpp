#include "Program/PanelWindow.h"

#include "Program/Gui/GuiDrawer.h"
#include "Program/Gui/GuiTheme.h"
#include "Program/Language.h"
#include "Program/MenuBar.h"

#include <algorithm>
#include <cstdlib>
#include <utility>
#include <windowsx.h>

namespace Chrivent {
	LRESULT CALLBACK PanelWindow::WindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		auto* panelWindow = reinterpret_cast<PanelWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (msg == WM_NCCREATE) {
			const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			panelWindow = static_cast<PanelWindow*>(create->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panelWindow));
			panelWindow->window = hwnd;
		}
		if (!panelWindow)
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		switch (msg) {
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT: {
			PAINTSTRUCT paint{};
			const HDC deviceContext = BeginPaint(hwnd, &paint);
			RECT client{};
			GetClientRect(hwnd, &client);
			const int width = std::max(0, static_cast<int>(client.right - client.left));
			const int height = std::max(0, static_cast<int>(client.bottom - client.top));
			const HDC memoryContext = CreateCompatibleDC(deviceContext);
			const HBITMAP bitmap = width > 0 && height > 0 ? CreateCompatibleBitmap(deviceContext, width, height) : nullptr;
			if (memoryContext && bitmap) {
				const HGDIOBJ previousBitmap = SelectObject(memoryContext, bitmap);
				panelWindow->Paint(memoryContext);
				BitBlt(deviceContext, 0, 0, width, height, memoryContext, 0, 0, SRCCOPY);
				SelectObject(memoryContext, previousBitmap);
			} else
				panelWindow->Paint(deviceContext);
			if (bitmap)
				DeleteObject(bitmap);
			if (memoryContext)
				DeleteDC(memoryContext);
			EndPaint(hwnd, &paint);
			return 0;
		}
		case WM_CTLCOLORBTN:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX:
		case WM_CTLCOLORSTATIC:
			return GuiTheme::HandleControlColor(msg, wParam, lParam);
		case WM_COMMAND:
			if (panelWindow->menuBar && panelWindow->menuBar->HandleCommand(LOWORD(wParam))) {
				if (panelWindow->menuBar->ConsumePanelLayoutResetRequest())
					panelWindow->ResetPanelLayout();
				return 0;
			}
			for (const auto& entry : panelWindow->panels) {
				if (entry.panel && entry.panel->HandleCommand(LOWORD(wParam), HIWORD(wParam)))
					return 0;
			}
			break;
		case WM_NOTIFY: {
			const auto* notifyHeader = reinterpret_cast<NMHDR*>(lParam);
			if (!notifyHeader)
				break;
			LRESULT notifyResult = 0;
			for (const auto& entry : panelWindow->panels) {
				if (entry.panel && entry.panel->HandleNotify(*notifyHeader, notifyResult))
					return notifyResult;
			}
			break;
		}
		case WM_HSCROLL:
		case WM_VSCROLL:
			for (const auto& entry : panelWindow->panels) {
				if (entry.panel && entry.panel->HandleScroll(reinterpret_cast<HWND>(lParam), LOWORD(wParam)))
					return 0;
			}
			break;
		case WM_SIZE:
			panelWindow->LayoutPanels();
			return 0;
		case WM_ENTERMENULOOP:
			panelWindow->StartMenuFrameTimer();
			break;
		case WM_EXITMENULOOP:
			panelWindow->StopMenuFrameTimer();
			break;
		case WM_EXITSIZEMOVE:
			panelWindow->NotifyInteractionFinished();
			break;
		case WM_TIMER:
			if (wParam == kMenuFrameTimerId) {
				if (!panelWindow->RenderMenuFrame())
					panelWindow->closeRequested = true;
				return 0;
			}
			break;
		case WM_LBUTTONDOWN:
			panelWindow->dragBoundary = panelWindow->HitTestBoundary(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (panelWindow->dragBoundary != DragBoundary::None)
				SetCapture(hwnd);
			return 0;
		case WM_MOUSEMOVE:
			if (panelWindow->dragBoundary != DragBoundary::None && (wParam & MK_LBUTTON)) {
				panelWindow->MoveBoundary(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				return 0;
			}
			break;
		case WM_LBUTTONUP:
			if (panelWindow->dragBoundary != DragBoundary::None) {
				panelWindow->MoveBoundary(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				panelWindow->dragBoundary = DragBoundary::None;
				ReleaseCapture();
				Settings::SavePanelLayout(panelWindow->layoutSettings);
				panelWindow->NotifyInteractionFinished();
				return 0;
			}
			break;
		case WM_CAPTURECHANGED:
			if (panelWindow->dragBoundary != DragBoundary::None)
				panelWindow->NotifyInteractionFinished();
			panelWindow->dragBoundary = DragBoundary::None;
			return 0;
		case WM_SETCURSOR:
			if (LOWORD(lParam) == HTCLIENT) {
				POINT cursor{};
				GetCursorPos(&cursor);
				ScreenToClient(hwnd, &cursor);
				const DragBoundary boundary = panelWindow->HitTestBoundary(cursor.x, cursor.y);
				if (boundary == DragBoundary::Left || boundary == DragBoundary::Right) {
					SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32644)));
					return TRUE;
				}
				if (boundary == DragBoundary::Bottom) {
					SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32645)));
					return TRUE;
				}
			}
			break;
		case WM_CLOSE:
			panelWindow->closeRequested = true;
			return 0;
		case WM_DESTROY:
			panelWindow->StopMenuFrameTimer();
			panelWindow->window = nullptr;
			for (auto& entry : panelWindow->panels)
				entry.frame = nullptr;
			return 0;
		default:
			break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void PanelWindow::StartMenuFrameTimer() const {
		if (window && menuFrameCallback)
			SetTimer(window, kMenuFrameTimerId, 16, nullptr);
	}

	void PanelWindow::StopMenuFrameTimer() const {
		if (window)
			KillTimer(window, kMenuFrameTimerId);
	}

	bool PanelWindow::RenderMenuFrame() const {
		return !menuFrameCallback || menuFrameCallback();
	}

	void PanelWindow::NotifyInteractionFinished() const {
		if (interactionFinishedCallback)
			interactionFinishedCallback();
	}

	void PanelWindow::CreatePanelControls() {
		for (auto& entry : panels) {
			if (!entry.panel || entry.frame)
				continue;
			entry.frame = CreateWindowExW(
				0, L"STATIC", Language::Text(entry.titleKey).c_str(),
				WS_CHILD | WS_VISIBLE | SS_LEFT,
				0, 0, 0, 0,
				window, nullptr, GetModuleHandleW(nullptr), nullptr);
			GuiTheme::ApplyControl(entry.frame);
			entry.panel->Create(window);
			ShowWindow(entry.frame, entry.visible ? SW_SHOW : SW_HIDE);
			entry.panel->UpdateVisibility(entry.visible);
		}
	}

	void PanelWindow::Paint(const HDC deviceContext) const {
		RECT client{};
		GetClientRect(window, &client);
		GuiDrawer::FillRectColor(deviceContext, client, GuiTheme::backgroundColor);
		for (auto& entry : panels) {
			if (!entry.visible || IsRectEmpty(&entry.bounds))
				continue;
			const auto& [left, top, right, bottom] = entry.bounds;
			GuiDrawer::DrawLine(deviceContext, left, top, right - 1, top, GuiTheme::borderColor);
			GuiDrawer::DrawLine(deviceContext, right - 1, top, right - 1, bottom - 1, GuiTheme::borderColor);
			GuiDrawer::DrawLine(deviceContext, right - 1, bottom - 1, left, bottom - 1, GuiTheme::borderColor);
			GuiDrawer::DrawLine(deviceContext, left, bottom - 1, left, top, GuiTheme::borderColor);
		}
	}

	void PanelWindow::LayoutPanels() {
		RECT client{};
		GetClientRect(window, &client);
		const int width = client.right - client.left;
		const int height = client.bottom - client.top;
		if (width <= 0 || height <= 0)
			return;
		constexpr int margin = 10;
		constexpr int gap = 8;
		const int maximumBottomHeight = std::max(100, height - margin * 2 - gap - 120);
		if (layoutSettings.bottomHeight <= 0)
			layoutSettings.bottomHeight = std::max(120, height / 4);
		const int bottomHeight = std::clamp(layoutSettings.bottomHeight, 100, maximumBottomHeight);
		layoutSettings.bottomHeight = bottomHeight;
		const int topHeight = std::max(0, height - bottomHeight - margin * 2 - gap);
		if (layoutSettings.leftWidth <= 0)
			layoutSettings.leftWidth = std::clamp(width / 4, 160, 320);
		if (layoutSettings.rightWidth <= 0)
			layoutSettings.rightWidth = std::clamp(width / 4, 160, 320);
		const int maximumSideWidth = std::max(120, width - margin * 2 - gap * 2 - 240 - 120);
		const int leftWidth = std::clamp(layoutSettings.leftWidth, 120, maximumSideWidth);
		const int maximumRightWidth = std::max(120, width - margin * 2 - gap * 2 - leftWidth - 240);
		const int rightWidth = std::clamp(layoutSettings.rightWidth, 120, maximumRightWidth);
		layoutSettings.leftWidth = leftWidth;
		layoutSettings.rightWidth = rightWidth;
		const int motionX = margin + leftWidth + gap;
		const int interpolationX = width - margin - rightWidth;
		const int motionWidth = std::max(0, interpolationX - gap - motionX);
		const int bottomY = margin + topHeight + gap;
		bool informationVisible = false;
		for (const auto& entry : panels) {
			if (entry.visible && entry.area == PanelWindowArea::Information) {
				informationVisible = true;
				break;
			}
		}
		const int informationHeight = informationVisible && topHeight > gap
			? std::min(topHeight - gap, std::clamp(topHeight / 3, 100, 220)) : 0;
		const int modelHeight = std::max(0, topHeight - (informationHeight > 0 ? informationHeight + gap : 0));
		int bottomIndex = 0;
		int bottomCount = 0;
		for (const auto& entry : panels) {
			if (entry.visible && entry.area == PanelWindowArea::Bottom)
				bottomCount++;
		}
		for (auto& entry : panels) {
			if (!entry.panel || !entry.frame || !entry.visible)
				continue;
			RECT area{};
			switch (entry.area) {
			case PanelWindowArea::Model:
				area = { .left = margin, .top = margin, .right = margin + leftWidth, .bottom = margin + modelHeight };
				break;
			case PanelWindowArea::Information:
				area = { .left = margin, .top = margin + modelHeight + gap, .right = margin + leftWidth, .bottom = margin + topHeight };
				break;
			case PanelWindowArea::Motion:
				area = { .left = motionX, .top = margin, .right = motionX + motionWidth, .bottom = margin + topHeight };
				break;
			case PanelWindowArea::InterpolationCurve:
				area = { .left = interpolationX, .top = margin, .right = interpolationX + rightWidth, .bottom = margin + topHeight };
				break;
			case PanelWindowArea::Bottom: {
				const int panelWidth = bottomCount > 0 ? (width - margin * 2 - gap * (bottomCount - 1)) / bottomCount : 0;
				const int x = margin + bottomIndex * (panelWidth + gap);
				area = {.left = x, .top = bottomY, .right = x + panelWidth, .bottom = bottomY + bottomHeight};
				bottomIndex++;
				break;
			}
			}
			const int areaWidth = std::max(0, static_cast<int>(area.right - area.left));
			entry.bounds = area;
			MoveWindow(entry.frame, area.left + 8, area.top + 5, areaWidth - 16, 18, TRUE);
			RECT panelRect{.left = area.left + 4, .top = area.top + 24, .right = area.right - 4, .bottom = area.bottom - 4};
			entry.panel->Resize(panelRect);
		}
		InvalidateRect(window, nullptr, FALSE);
	}

	PanelWindow::DragBoundary PanelWindow::HitTestBoundary(const int x, const int y) const {
		if (!window)
			return DragBoundary::None;
		RECT client{};
		GetClientRect(window, &client);
		constexpr int margin = 10;
		constexpr int gap = 8;
		constexpr int hitRange = 5;
		const int leftBoundary = margin + layoutSettings.leftWidth + gap / 2;
		const int rightBoundary = client.right - margin - layoutSettings.rightWidth - gap / 2;
		const int bottomBoundary = client.bottom - margin - layoutSettings.bottomHeight - gap / 2;
		if (y < bottomBoundary && std::abs(x - leftBoundary) <= hitRange)
			return DragBoundary::Left;
		if (y < bottomBoundary && std::abs(x - rightBoundary) <= hitRange)
			return DragBoundary::Right;
		if (std::abs(y - bottomBoundary) <= hitRange)
			return DragBoundary::Bottom;
		return DragBoundary::None;
	}

	void PanelWindow::MoveBoundary(const int x, const int y) {
		if (!window)
			return;
		RECT client{};
		GetClientRect(window, &client);
		constexpr int margin = 10;
		switch (dragBoundary) {
		case DragBoundary::Left:
			layoutSettings.leftWidth = x - margin;
			break;
		case DragBoundary::Right:
			layoutSettings.rightWidth = client.right - margin - x;
			break;
		case DragBoundary::Bottom:
			layoutSettings.bottomHeight = client.bottom - margin - y;
			break;
		case DragBoundary::None:
			return;
		}
		LayoutPanels();
		RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_NOERASE);
	}

	void PanelWindow::ResetPanelLayout() {
		layoutSettings = {};
		Settings::ResetPanelLayout();
		LayoutPanels();
		RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_UPDATENOW);
	}

	PanelWindow::~PanelWindow() {
		Destroy();
	}

	void PanelWindow::AttachMenuBar(MenuBar& menu) {
		menuBar = &menu;
	}

	void PanelWindow::RegisterPanel(
		Panel& panel, std::string titleKey, const PanelWindowArea area, const bool visible) {
		panels.push_back({ &panel, std::move(titleKey), area, nullptr, {}, visible });
	}

	bool PanelWindow::Show() {
		if (window) {
			ShowWindow(window, SW_MAXIMIZE);
			SetForegroundWindow(window);
			return true;
		}
		closeRequested = false;
		layoutSettings = Settings::LoadPanelLayout();
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		wc.hbrBackground = GuiTheme::GetBackgroundBrush();
		wc.lpszClassName = L"PmxModPanelWindow";
		RegisterClassExW(&wc);
		window = CreateWindowExW(
			0, L"PmxModPanelWindow", Language::Text("window.settings").c_str(),
			WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
			CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
			nullptr, nullptr, instance, this);
		if (!window)
			return false;
		if (menuBar) {
			const HMENU menuHandle = CreateMenu();
			if (!menuHandle) {
				Destroy();
				return false;
			}
			menuBar->SetOwnerWindow(window);
			menuBar->AddMenu(menuHandle);
			if (!SetMenu(window, menuHandle)) {
				DestroyMenu(menuHandle);
				Destroy();
				return false;
			}
			DrawMenuBar(window);
		}
		CreatePanelControls();
		GuiTheme::ApplyWindow(window);
		LayoutPanels();
		ShowWindow(window, SW_MAXIMIZE);
		UpdateWindow(window);
		return true;
	}

	void PanelWindow::UpdatePanelVisibility(const Panel& panel, const bool visible) {
		for (auto& entry : panels) {
			if (entry.panel != &panel || entry.visible == visible)
				continue;
			entry.visible = visible;
			entry.bounds = {};
			if (entry.frame)
				ShowWindow(entry.frame, visible ? SW_SHOW : SW_HIDE);
			entry.panel->UpdateVisibility(visible);
			LayoutPanels();
			RedrawWindow(window, nullptr, nullptr,
				RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
			return;
		}
	}

	void PanelWindow::Poll() const {
		MSG msg{};
		while (PeekMessageW(&msg, window, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	void PanelWindow::RefreshLanguage() const {
		if (!window)
			return;
		SetWindowTextW(window, Language::Text("window.settings").c_str());
		if (menuBar) {
			const HMENU previousMenu = GetMenu(window);
			const HMENU menuHandle = CreateMenu();
			menuBar->AddMenu(menuHandle);
			SetMenu(window, menuHandle);
			if (previousMenu)
				DestroyMenu(previousMenu);
		}
		for (auto& entry : panels) {
			if (entry.frame)
				SetWindowTextW(entry.frame, Language::Text(entry.titleKey).c_str());
			if (entry.panel)
				entry.panel->UpdateLanguage();
		}
		GuiTheme::ApplyWindow(window);
		DrawMenuBar(window);
	}

	void PanelWindow::Destroy() {
		StopMenuFrameTimer();
		for (auto& entry : panels) {
			if (entry.panel)
				entry.panel->Destroy();
			entry.frame = nullptr;
		}
		if (window) {
			const HMENU menuHandle = GetMenu(window);
			if (menuHandle)
				SetMenu(window, nullptr);
			DestroyWindow(window);
			if (menuHandle)
				DestroyMenu(menuHandle);
		}
		if (menuBar)
			menuBar->SetOwnerWindow(nullptr);
		window = nullptr;
		closeRequested = false;
	}
}
