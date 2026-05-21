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
		: viewerMenu(sceneConfigStorage) {
		playbackPanel.SetControlIds(kPlaybackTimelineSliderId, kPlaybackPlayButtonId, kPlaybackPauseButtonId, kPlaybackStopButtonId);
		soundPanel.SetVolumeSliderId(kSoundVolumeSliderId);
		panelWindow.RegisterPanel(playbackPanel, L"Playback", PanelWindowArea::Playback);
		panelWindow.RegisterPanel(soundPanel, L"Sound", PanelWindowArea::Bottom);
		Reset();
	}

	PanelManager::~PanelManager() {
		DestroyGui();
	}

	void PanelManager::AttachRenderWindow(const ViewerInfo& viewerInfo) {
		renderWindow = glfwGetWin32Window(viewerInfo.window);
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

	bool PanelManager::OpenGuiWindows() {
		panelWindow.Show();
		return true;
	}

	void PanelManager::PollGuiWindows() const {
		panelWindow.Poll();
	}

	void PanelManager::DestroyGui() {
		if (renderWindow && prevRenderWindowProc) {
			SetWindowLongPtrW(renderWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(prevRenderWindowProc));
			RemovePropW(renderWindow, L"PmxModPanelManager");
			prevRenderWindowProc = nullptr;
		}
		renderWindow = nullptr;
		panelWindow.Destroy();
	}
}
