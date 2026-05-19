#include "PanelManager.h"

#include "../Sound.h"

namespace Chrivent {
	void PanelManager::ResizeControlWindow() {
		if (!controlWindow)
			return;
		RECT client{};
		GetClientRect(controlWindow, &client);
		scenePanel.Resize(client);
	}

	LRESULT CALLBACK PanelManager::ControlWindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		auto* panelManager = reinterpret_cast<PanelManager*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (msg == WM_NCCREATE) {
			const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
			panelManager = static_cast<PanelManager*>(create->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panelManager));
			panelManager->controlWindow = hwnd;
		}
		if (!panelManager)
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		switch (msg) {
			case WM_CREATE: {
				const HMENU menu = CreateMenu();
				panelManager->scenePanel.AddMenu(menu);
				SetMenu(hwnd, menu);
				panelManager->scenePanel.Create(hwnd);
				panelManager->ResizeControlWindow();
				return 0;
			}
			case WM_SIZE:
				panelManager->ResizeControlWindow();
				return 0;
			case WM_COMMAND:
				if (panelManager->scenePanel.HandleCommand(LOWORD(wParam)))
					return 0;
				break;
			case WM_APP + 2:
				ShowWindow(hwnd, SW_SHOWNORMAL);
				SetForegroundWindow(hwnd);
				return 0;
			case WM_CLOSE:
				ShowWindow(hwnd, SW_HIDE);
				return 0;
			case WM_DESTROY:
				panelManager->scenePanel.Destroy();
				panelManager->soundPanel.Destroy();
				panelManager->controlWindow = nullptr;
				return 0;
			default:
				break;
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	PanelManager::PanelManager()
		: scenePanel(sceneConfigStorage), sceneConfig(sceneConfigStorage) {
		Reset();
	}

	PanelManager::~PanelManager() {
		DestroyPanelWindows();
	}

	void PanelManager::ApplySceneConfig(const SceneConfig& cfg) {
		scenePanel.ApplySceneConfig(cfg);
	}

	void PanelManager::Reset() {
		scenePanel.Reset();
	}

	bool PanelManager::OpenPanelWindows() {
		if (controlWindow) {
			ShowWindow(controlWindow, SW_SHOWNORMAL);
			soundPanel.Show();
			return true;
		}
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = ControlWindowProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = L"PmxModPanelWindow";
		RegisterClassExW(&wc);
		controlWindow = CreateWindowExW(
			0, L"PmxModPanelWindow", L"PmxMod Panel",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 420, 220,
			nullptr, nullptr, instance, this);
		if (!controlWindow)
			return false;
		ShowWindow(controlWindow, SW_SHOWNORMAL);
		UpdateWindow(controlWindow);
		soundPanel.Show();
		return true;
	}

	void PanelManager::PollPanelWindows() const {
		MSG msg{};
		if (controlWindow) {
			while (PeekMessageW(&msg, controlWindow, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
		soundPanel.Poll();
	}

	void PanelManager::DestroyPanelWindows() {
		if (controlWindow)
			DestroyWindow(controlWindow);
		controlWindow = nullptr;
		scenePanel.Destroy();
		soundPanel.Destroy();
	}

	bool PanelManager::ConsumeSceneConfigDirty() {
		return scenePanel.ConsumeSceneConfigDirty();
	}

	void PanelManager::BindSound(Sound& sound) {
		soundPanel.BindSound(sound);
	}
}
