#include "GuiManager.h"

#include "../Sound.h"
#include "../../Viewer/Viewer.h"

#include <CommCtrl.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Chrivent {
	LRESULT CALLBACK GuiManager::RenderWindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) {
		auto* guiManager = static_cast<GuiManager*>(GetPropW(hwnd, L"PmxModGuiManager"));
		if (guiManager && msg == WM_COMMAND) {
			if (guiManager->viewerMenu.HandleCommand(LOWORD(wParam)))
				return 0;
		}
		if (guiManager && guiManager->prevRenderWindowProc)
			return CallWindowProcW(guiManager->prevRenderWindowProc, hwnd, msg, wParam, lParam);
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	GuiManager::GuiManager()
		: viewerMenu(sceneConfigStorage), sceneConfig(sceneConfigStorage) {
		playbackPanel.SetControlIds(kPlaybackTimelineSliderId, kPlaybackPlayButtonId, kPlaybackPauseButtonId, kPlaybackStopButtonId);
		soundPanel.SetVolumeSliderId(kSoundVolumeSliderId);
		Reset();
	}

	GuiManager::~GuiManager() {
		DestroyGui();
	}

	void GuiManager::AttachRenderWindow(const Viewer& viewer) {
		renderWindow = glfwGetWin32Window(viewer.window);
		if (!renderWindow)
			return;
		viewerMenu.AttachOwner(renderWindow);
		const HMENU menu = CreateMenu();
		viewerMenu.AddMenu(menu);
		SetMenu(renderWindow, menu);
		DrawMenuBar(renderWindow);
		SetPropW(renderWindow, L"PmxModGuiManager", this);
		prevRenderWindowProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(renderWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RenderWindowProc)));
	}

	void GuiManager::ApplySceneConfig(const SceneConfig& cfg) {
		viewerMenu.ApplySceneConfig(cfg);
	}

	void GuiManager::Reset() {
		viewerMenu.Reset();
	}

	bool GuiManager::OpenGuiWindows() {
		playbackPanel.Show();
		soundPanel.Show();
		return true;
	}

	void GuiManager::PollGuiWindows() const {
		playbackPanel.Poll();
		soundPanel.Poll();
	}

	void GuiManager::DestroyGui() {
		if (renderWindow && prevRenderWindowProc) {
			SetWindowLongPtrW(renderWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(prevRenderWindowProc));
			RemovePropW(renderWindow, L"PmxModGuiManager");
			prevRenderWindowProc = nullptr;
		}
		renderWindow = nullptr;
		playbackPanel.Destroy();
		soundPanel.Destroy();
	}

	bool GuiManager::ConsumeSceneConfigDirty() {
		return viewerMenu.ConsumeSceneConfigDirty();
	}

	PlaybackCommand GuiManager::ConsumePlaybackCommand() {
		return playbackPanel.ConsumeCommand();
	}

	bool GuiManager::ConsumeSeekFrame(int& frame) {
		return playbackPanel.ConsumeSeekFrame(frame);
	}

	void GuiManager::SetPlaybackFrame(const int frame) const {
		playbackPanel.SetCurrentFrame(frame);
	}

	void GuiManager::SetPlaybackFrameRange(const int maxFrame) const {
		playbackPanel.SetFrameRange(maxFrame);
	}

	void GuiManager::BindSound(Sound& sound) {
		soundPanel.BindSound(sound);
	}

	HWND GuiManager::CreateHorizontalSlider(const HWND parent, const int controlId, const int minValue, const int maxValue, const int initialValue) {
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

	HWND GuiManager::CreateVerticalTickSlider(
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
