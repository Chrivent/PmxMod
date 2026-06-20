#include "PanelWindow.h"

#include "Language.h"
#include "MenuBar.h"

#include <utility>

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
			case WM_COMMAND:
				if (panelWindow->menuBar && panelWindow->menuBar->HandleCommand(LOWORD(wParam)))
					return 0;
				for (const auto& entry : panelWindow->panels) {
					if (entry.panel && entry.panel->HandleCommand(LOWORD(wParam), HIWORD(wParam)))
						return 0;
				}
				break;
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
			case WM_CLOSE:
				panelWindow->closeRequested = true;
				return 0;
			case WM_DESTROY:
				panelWindow->window = nullptr;
				for (auto& entry : panelWindow->panels)
					entry.frame = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	void PanelWindow::CreatePanelControls() {
		for (auto& entry : panels) {
			if (!entry.panel || entry.frame)
				continue;
			entry.frame = CreateWindowExW(
				0, L"BUTTON", Language::Text(entry.titleKey).c_str(),
				WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
				0, 0, 0, 0,
				window, nullptr, GetModuleHandleW(nullptr), nullptr);
			entry.panel->Create(window);
		}
	}

	void PanelWindow::LayoutPanels() const {
		RECT client{};
		GetClientRect(window, &client);
		const int width = client.right - client.left;
		const int height = client.bottom - client.top;
		if (width <= 0 || height <= 0)
			return;
		constexpr int margin = 10;
		constexpr int gap = 8;
		const int bottomHeight = (std::max)(120, height / 4);
		const int topHeight = (std::max)(0, height - bottomHeight - margin * 2 - gap);
		const int sideWidth = std::clamp(width / 4, 160, 320);
		const int motionX = margin + sideWidth + gap;
		const int interpolationX = width - margin - sideWidth;
		const int motionWidth = (std::max)(0, interpolationX - gap - motionX);
		const int bottomY = margin + topHeight + gap;
		int bottomIndex = 0;
		int bottomCount = 0;
		for (const auto& entry : panels) {
			if (entry.area == PanelWindowArea::Bottom)
				bottomCount++;
		}
		for (const auto& entry : panels) {
			if (!entry.panel || !entry.frame)
				continue;
			RECT area{};
			switch (entry.area) {
				case PanelWindowArea::Model:
					area = {margin, margin, margin + sideWidth, margin + topHeight};
					break;
				case PanelWindowArea::Motion:
					area = {motionX, margin, motionX + motionWidth, margin + topHeight};
					break;
				case PanelWindowArea::InterpolationCurve:
					area = {
						interpolationX,
						margin,
						interpolationX + sideWidth,
						margin + topHeight
					};
					break;
				case PanelWindowArea::Bottom: {
					const int panelWidth = bottomCount > 0
						? (width - margin * 2 - gap * (bottomCount - 1)) / bottomCount
						: 0;
					const int x = margin + bottomIndex * (panelWidth + gap);
					area = {x, bottomY, x + panelWidth, bottomY + bottomHeight};
					bottomIndex++;
					break;
				}
			}
			const int areaWidth = (std::max)(0, static_cast<int>(area.right - area.left));
			const int areaHeight = (std::max)(0, static_cast<int>(area.bottom - area.top));
			MoveWindow(entry.frame, area.left, area.top, areaWidth, areaHeight, TRUE);
			RECT panelRect{area.left, area.top + 18, area.right, area.bottom};
			entry.panel->Resize(panelRect);
		}
	}

	PanelWindow::~PanelWindow() {
		Destroy();
	}

	void PanelWindow::AttachMenuBar(MenuBar& menu) {
		menuBar = &menu;
		menuBar->SetOwnerWindow(window);
		const HMENU menuHandle = CreateMenu();
		menuBar->AddMenu(menuHandle);
		SetMenu(window, menuHandle);
		DrawMenuBar(window);
	}

	void PanelWindow::RegisterPanel(Panel& panel, std::string titleKey, const PanelWindowArea area) {
		panels.push_back({ &panel, std::move(titleKey), area });
	}

	void PanelWindow::Show() {
		closeRequested = false;
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = L"PmxModPanelWindow";
		RegisterClassExW(&wc);
		window = CreateWindowExW(
			0, L"PmxModPanelWindow", Language::Text("window.settings").c_str(),
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
			nullptr, nullptr, instance, this);
		if (menuBar) {
			menuBar->SetOwnerWindow(window);
			const HMENU menuHandle = CreateMenu();
			menuBar->AddMenu(menuHandle);
			SetMenu(window, menuHandle);
			DrawMenuBar(window);
		}
		CreatePanelControls();
		LayoutPanels();
		ShowWindow(window, SW_MAXIMIZE);
		UpdateWindow(window);
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
		DrawMenuBar(window);
	}

	void PanelWindow::Destroy() {
		for (auto& entry : panels) {
			if (entry.panel)
				entry.panel->Destroy();
			entry.frame = nullptr;
		}
		if (window)
			DestroyWindow(window);
		closeRequested = false;
	}
}
