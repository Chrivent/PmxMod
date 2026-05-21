#include "ViewerMenu.h"

#include <iostream>

namespace Chrivent {
	ViewerMenu::ViewerMenu(SceneConfig& config) : sceneConfig(config) {}

	void ViewerMenu::AddMenu(const HMENU menu) {
		HMENU fileMenu = CreatePopupMenu();
		AppendMenuW(fileMenu, MF_STRING, kOpenButtonId, L"Open...");
		AppendMenuW(fileMenu, MF_STRING, kSaveButtonId, L"Save...");
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");
	}

	void ViewerMenu::AttachOwner(const HWND owner) {
		ownerWindow = owner;
	}

	bool ViewerMenu::SaveSceneConfig(const std::filesystem::path& filepath) const {
		return sceneConfig.Save(filepath);
	}

	bool ViewerMenu::LoadSceneConfig(const std::filesystem::path& filepath) {
		if (!sceneConfig.Load(filepath))
			return false;
		sceneFilePath = filepath;
		sceneConfigDirty = true;
		return true;
	}

	void ViewerMenu::ShowOpenSceneDialog() {
		std::vector filename(MAX_PATH, L'\0');
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = ownerWindow;
		ofn.lpstrFilter = L"PmxMod Scene (*.pms)\0*.pms\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = filename.size();
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pms";
		if (!GetOpenFileNameW(&ofn))
			return;
		if (LoadSceneConfig(filename.data()))
			std::cout << "Scene config loaded.\n";
		else
			std::cout << "Failed to load scene config.\n";
	}

	void ViewerMenu::ShowSaveSceneDialog() {
		std::vector filename(MAX_PATH, L'\0');
		if (!sceneFilePath.empty()) {
			const auto native = sceneFilePath.wstring();
			std::wcsncpy(filename.data(), native.c_str(), filename.size() - 1);
		}
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = ownerWindow;
		ofn.lpstrFilter = L"PmxMod Scene (*.pms)\0*.pms\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = static_cast<DWORD>(filename.size());
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pms";
		if (!GetSaveFileNameW(&ofn))
			return;
		sceneFilePath = filename.data();
		if (SaveSceneConfig(sceneFilePath))
			std::cout << "Current scene config saved.\n";
		else
			std::cout << "Failed to save scene config.\n";
	}

	bool ViewerMenu::HandleCommand(const int commandId) {
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

	void ViewerMenu::ApplySceneConfig(const SceneConfig& cfg) {
		sceneConfig = cfg;
		sceneFilePath.clear();
		sceneConfigDirty = false;
	}

	void ViewerMenu::Reset() {
		sceneConfigDirty = false;
	}

	bool ViewerMenu::ConsumeSceneConfigDirty() {
		const bool dirty = sceneConfigDirty;
		sceneConfigDirty = false;
		return dirty;
	}
}
