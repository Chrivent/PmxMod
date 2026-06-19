#include "PanelManager.h"

#include "../Sound.h"
#include "../../Viewer/Viewer.h"

namespace Chrivent {
	PanelManager::PanelManager()
		: menuBar(sceneConfigStorage) {
		playbackPanel.SetControlIds(kPlaybackTimelineSliderId, kPlaybackPlayButtonId, kPlaybackPauseButtonId, kPlaybackStopButtonId);
		soundPanel.SetVolumeSliderId(kSoundVolumeSliderId);
		panelWindow.AttachMenuBar(menuBar);
		panelWindow.RegisterPanel(modelPanel, L"Model", PanelWindowArea::Model);
		panelWindow.RegisterPanel(motionPanel, L"Motion", PanelWindowArea::Motion);
		panelWindow.RegisterPanel(playbackPanel, L"Playback", PanelWindowArea::Playback);
		panelWindow.RegisterPanel(soundPanel, L"Sound", PanelWindowArea::Bottom);
		Reset();
	}

	PanelManager::~PanelManager() {
		DestroyGui();
	}

	void PanelManager::AttachRenderWindow(const ViewerInfo& viewerInfo) {
		renderWindow = viewerInfo.window;
		menuBar.SetRenderWindowVisible(true);
	}

	void PanelManager::UpdateRenderWindow() {
		if (!renderWindow)
			return;
		if (glfwWindowShouldClose(renderWindow)) {
			glfwSetWindowShouldClose(renderWindow, GLFW_FALSE);
			glfwHideWindow(renderWindow);
			menuBar.SetRenderWindowVisible(false);
		}
		if (!menuBar.ConsumeRenderWindowOpenRequested())
			return;
		glfwShowWindow(renderWindow);
		glfwFocusWindow(renderWindow);
		menuBar.SetRenderWindowVisible(true);
	}

	bool PanelManager::OpenGuiWindows() {
		panelWindow.Show();
		return true;
	}

	void PanelManager::PollGuiWindows() const {
		panelWindow.Poll();
	}

	void PanelManager::DestroyGui() {
		renderWindow = nullptr;
		panelWindow.Destroy();
	}
}
