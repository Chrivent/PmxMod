#include "PanelManager.h"

#include "../Sound.h"

namespace Chrivent {
	void PanelManager::UpdateModelPanel() {
		std::vector<std::filesystem::path> modelPaths;
		modelPaths.reserve(sceneConfigStorage.modelConfigs.size());
		for (const auto& modelConfig : sceneConfigStorage.modelConfigs)
			modelPaths.emplace_back(modelConfig.modelPath);
		modelPanel.UpdateModelPaths(modelPaths);
	}

	PanelManager::PanelManager()
		: menuBar(sceneConfigStorage) {
		playbackPanel.SetControlIds({
			.playButton = kPlaybackPlayButtonId,
			.pauseButton = kPlaybackPauseButtonId,
			.stopButton = kPlaybackStopButtonId,
			.startFrameEdit = kPlaybackStartFrameEditId,
			.endFrameEdit = kPlaybackEndFrameEditId,
			.resetRangeButton = kPlaybackResetRangeButtonId,
			.repeatCheck = kPlaybackRepeatCheckId
		});
		soundPanel.SetVolumeSliderId(kSoundVolumeSliderId);
		modelPanel.SetControlIds(kModelAddButtonId, kModelListId);
		panelWindow.AttachMenuBar(menuBar);
		panelWindow.RegisterPanel(modelPanel, L"Model", PanelWindowArea::Model);
		panelWindow.RegisterPanel(motionPanel, L"Motion", PanelWindowArea::Motion);
		panelWindow.RegisterPanel(playbackPanel, L"Playback", PanelWindowArea::Bottom);
		panelWindow.RegisterPanel(soundPanel, L"Sound", PanelWindowArea::Bottom);
		Reset();
	}

	PanelManager::~PanelManager() {
		DestroyGui();
	}

	void PanelManager::ApplySceneConfig(const SceneConfig& cfg) {
		menuBar.ApplySceneConfig(cfg);
		UpdateModelPanel();
	}

	void PanelManager::CommitSceneConfig(const SceneConfig& cfg) {
		sceneConfigStorage = cfg;
		UpdateModelPanel();
	}

	bool PanelManager::OpenGuiWindows() {
		panelWindow.Show();
		return true;
	}

	void PanelManager::PollGuiWindows() const {
		panelWindow.Poll();
	}

	void PanelManager::DestroyGui() {
		panelWindow.Destroy();
	}
}
