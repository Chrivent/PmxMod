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

	void PanelManager::UpdateSidePanelVisibility() {
		const bool cameraMode = IsCameraMode();
		modelPanel.UpdateVisibility(!cameraMode);
		cameraPanel.UpdateVisibility(cameraMode);
		panelWindow.UpdatePanelVisibility(modelPanel, !cameraMode);
		panelWindow.UpdatePanelVisibility(cameraPanel, cameraMode);
	}

	PanelManager::PanelManager() : menuBar(sceneConfigStorage) {
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
		modelPanel.ApplyControlIds(kModelAddButtonId, kModelListId);
		cameraPanel.SetShaderListId(kCameraShaderListId);
		panelWindow.AttachMenuBar(menuBar);
		panelWindow.RegisterPanel(modelPanel, "panel.model", PanelWindowArea::Model, false);
		panelWindow.RegisterPanel(cameraPanel, "panel.camera", PanelWindowArea::Model);
		panelWindow.RegisterPanel(motionPanel, "panel.motion", PanelWindowArea::Motion);
		panelWindow.RegisterPanel(interpolationCurvePanel, "panel.interpolation_curve", PanelWindowArea::InterpolationCurve);
		panelWindow.RegisterPanel(playbackPanel, "panel.playback", PanelWindowArea::Bottom);
		panelWindow.RegisterPanel(soundPanel, "panel.sound", PanelWindowArea::Bottom);
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

	void PanelManager::BindSound(Sound& sound) {
		soundPanel.BindSound(sound);
		motionPanel.AttachWaveform(sound.GetWaveform());
	}

	bool PanelManager::OpenGuiWindows() {
		panelWindow.Show();
		return true;
	}

	void PanelManager::PollGuiWindows() {
		panelWindow.Poll();
		UpdateSidePanelVisibility();
		InterpolationSelection selection;
		if (motionPanel.ConsumeInterpolationSelection(selection))
			interpolationCurvePanel.ApplySelection(std::move(selection));
	}

}
