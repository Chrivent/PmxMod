#pragma once

#include "PanelCommandId.h"
#include "ToolPanel.h"
#include "../Config.h"

namespace Chrivent {
	class ScenePanel final : public ToolPanel {
		static constexpr int kOpenButtonId = PanelCommandId::sceneBase + 1;
		static constexpr int kSaveButtonId = PanelCommandId::sceneBase + 2;

		SceneConfig& sceneConfig;
		std::filesystem::path sceneFilePath;
		bool sceneConfigDirty = false;
		HWND parentWindow = nullptr;
		HWND statusText = nullptr;

		void SetStatusText(const std::wstring& text) const;
		bool SaveSceneConfig(const std::filesystem::path& filepath) const;
		bool LoadSceneConfig(const std::filesystem::path& filepath);
		void ShowOpenSceneDialog();
		void ShowSaveSceneDialog();

	public:
		explicit ScenePanel(SceneConfig& config);

		void AddMenu(HMENU menu) override;
		void Create(HWND parent) override;
		void Resize(const RECT& clientRect) override;
		bool HandleCommand(int commandId) override;
		void Destroy() override;

		void ApplySceneConfig(const SceneConfig& cfg);
		void Reset();
		bool ConsumeSceneConfigDirty();
	};
}
