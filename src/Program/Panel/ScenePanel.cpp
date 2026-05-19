#include "ScenePanel.h"

namespace Chrivent {
	ScenePanel::ScenePanel(SceneConfig& config) : sceneConfig(config) {}

	void ScenePanel::AddMenu(const HMENU menu) {
		HMENU fileMenu = CreatePopupMenu();
		AppendMenuW(fileMenu, MF_STRING, kOpenButtonId, L"Open...");
		AppendMenuW(fileMenu, MF_STRING, kSaveButtonId, L"Save...");
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");
	}

	void ScenePanel::SetStatusText(const std::wstring& text) const {
		if (statusText)
			SetWindowTextW(statusText, text.c_str());
	}

	bool ScenePanel::SaveSceneConfig(const std::filesystem::path& filepath) const {
		return sceneConfig.Save(filepath);
	}

	bool ScenePanel::LoadSceneConfig(const std::filesystem::path& filepath) {
		if (!sceneConfig.Load(filepath))
			return false;
		sceneFilePath = filepath;
		sceneConfigDirty = true;
		return true;
	}

	void ScenePanel::ShowOpenSceneDialog() {
		std::vector filename(MAX_PATH, L'\0');
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = parentWindow;
		ofn.lpstrFilter = L"PmxMod Scene (*.pms)\0*.pms\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = static_cast<DWORD>(filename.size());
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pms";
		if (!GetOpenFileNameW(&ofn))
			return;
		if (LoadSceneConfig(filename.data()))
			SetStatusText(L"Scene config loaded.");
		else
			SetStatusText(L"Failed to load scene config.");
	}

	void ScenePanel::ShowSaveSceneDialog() {
		std::vector filename(MAX_PATH, L'\0');
		if (!sceneFilePath.empty()) {
			const auto native = sceneFilePath.wstring();
			std::wcsncpy(filename.data(), native.c_str(), filename.size() - 1);
		}
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = parentWindow;
		ofn.lpstrFilter = L"PmxMod Scene (*.pms)\0*.pms\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = static_cast<DWORD>(filename.size());
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pms";
		if (!GetSaveFileNameW(&ofn))
			return;
		sceneFilePath = filename.data();
		if (SaveSceneConfig(sceneFilePath))
			SetStatusText(L"Current scene config saved.");
		else
			SetStatusText(L"Failed to save scene config.");
	}

	void ScenePanel::Create(HWND parent) {
		parentWindow = parent;
		statusText = CreateWindowExW(
			0, L"STATIC", L"Open or save the current scene config.",
			WS_CHILD | WS_VISIBLE,
			0, 0, 0, 0,
			parent, nullptr, GetModuleHandleW(nullptr), nullptr);
	}

	void ScenePanel::Resize(const RECT& clientRect) {
		constexpr int x = 14;
		constexpr int y = 14;
		const int width = static_cast<int>(clientRect.right) - x * 2;
		if (statusText)
			MoveWindow(statusText, x, y, width, 64, TRUE);
	}

	bool ScenePanel::HandleCommand(const int commandId) {
		switch (commandId) {
			case kOpenButtonId:
				ShowOpenSceneDialog();
				return true;
			case kSaveButtonId:
				ShowSaveSceneDialog();
				return true;
			default:
				return false;
		}
	}

	void ScenePanel::Destroy() {
		statusText = nullptr;
		parentWindow = nullptr;
	}

	void ScenePanel::ApplySceneConfig(const SceneConfig& cfg) {
		sceneConfig = cfg;
		sceneFilePath.clear();
		sceneConfigDirty = false;
	}

	void ScenePanel::Reset() {
		sceneConfigDirty = false;
	}

	bool ScenePanel::ConsumeSceneConfigDirty() {
		const bool dirty = sceneConfigDirty;
		sceneConfigDirty = false;
		return dirty;
	}
}
