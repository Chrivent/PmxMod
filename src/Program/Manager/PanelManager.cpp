#include "PanelManager.h"

#include "../Sound.h"
#include "../../Viewer/Viewer.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

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
		renderWindow = glfwGetWin32Window(viewerInfo.window);
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
