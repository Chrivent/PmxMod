#include "PanelManager.h"

#include "../Sound.h"
#include "../../Viewer/Viewer.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	LRESULT CALLBACK PanelManager::RenderWindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		auto* panelManager = static_cast<PanelManager*>(GetPropW(hwnd, L"PmxModPanelManager"));
		if (panelManager && msg == WM_COMMAND) {
			if (panelManager->viewerMenu.HandleCommand(LOWORD(wParam)))
				return 0;
		}
		if (panelManager && panelManager->prevRenderWindowProc)
			return CallWindowProcW(panelManager->prevRenderWindowProc, hwnd, msg, wParam, lParam);
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	PanelManager::PanelManager()
		: viewerMenu(sceneConfigStorage), sceneConfig(sceneConfigStorage) {
		soundPanel.SetVolumeSliderId(kSoundVolumeSliderId);
		Reset();
	}

	PanelManager::~PanelManager() {
		DestroyPanelWindows();
	}

	void PanelManager::AttachRenderWindow(const Viewer& viewer) {
		renderWindow = glfwGetWin32Window(viewer.window);
		if (!renderWindow)
			return;
		viewerMenu.AttachOwner(renderWindow);
		const HMENU menu = CreateMenu();
		viewerMenu.AddMenu(menu);
		SetMenu(renderWindow, menu);
		DrawMenuBar(renderWindow);
		SetPropW(renderWindow, L"PmxModPanelManager", this);
		prevRenderWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(renderWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RenderWindowProc)));
	}

	void PanelManager::ApplySceneConfig(const SceneConfig& cfg) {
		viewerMenu.ApplySceneConfig(cfg);
	}

	void PanelManager::Reset() {
		viewerMenu.Reset();
	}

	bool PanelManager::OpenPanelWindows() {
		soundPanel.Show();
		return true;
	}

	void PanelManager::PollPanelWindows() const {
		soundPanel.Poll();
	}

	void PanelManager::DestroyPanelWindows() {
		if (renderWindow && prevRenderWindowProc) {
			SetWindowLongPtrW(renderWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(prevRenderWindowProc));
			RemovePropW(renderWindow, L"PmxModPanelManager");
			prevRenderWindowProc = nullptr;
		}
		renderWindow = nullptr;
		soundPanel.Destroy();
	}

	bool PanelManager::ConsumeSceneConfigDirty() {
		return viewerMenu.ConsumeSceneConfigDirty();
	}

	void PanelManager::BindSound(Sound& sound) {
		soundPanel.BindSound(sound);
	}
}
