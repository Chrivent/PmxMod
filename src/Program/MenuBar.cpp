#include "MenuBar.h"

#include <iostream>

namespace Chrivent {
	bool MenuBar::SaveSceneConfig(const std::filesystem::path& filepath) const {
		return sceneConfig.Save(filepath);
	}

	bool MenuBar::LoadSceneConfig(const std::filesystem::path& filepath) {
		if (!sceneConfig.Load(filepath))
			return false;
		sceneFilePath = filepath;
		sceneConfigDirty = true;
		return true;
	}

	void MenuBar::ShowOpenSceneDialog() {
		std::vector filename(MAX_PATH, L'\0');
		std::wstring filter = Language::Text("scene.dialog.scene");
		filter.append(1, L'\0');
		filter += L"*.pms";
		filter.append(1, L'\0');
		filter += Language::Text("dialog.all_files");
		filter.append(1, L'\0');
		filter += L"*.*";
		filter.append(2, L'\0');
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = ownerWindow;
		ofn.lpstrFilter = filter.c_str();
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = filename.size();
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pms";
		if (!GetOpenFileNameW(&ofn))
			return;
		if (LoadSceneConfig(filename.data()))
			std::cout << "Scene config loaded.\n";
		else
			std::cerr << "Failed to load scene config.\n";
	}

	void MenuBar::ShowSaveSceneDialog() {
		std::vector filename(MAX_PATH, L'\0');
		std::wstring filter = Language::Text("scene.dialog.scene");
		filter.append(1, L'\0');
		filter += L"*.pms";
		filter.append(1, L'\0');
		filter += Language::Text("dialog.all_files");
		filter.append(1, L'\0');
		filter += L"*.*";
		filter.append(2, L'\0');
		if (!sceneFilePath.empty()) {
			const auto native = sceneFilePath.wstring();
			std::wcsncpy(filename.data(), native.c_str(), filename.size() - 1);
		}
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = ownerWindow;
		ofn.lpstrFilter = filter.c_str();
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = filename.size();
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pms";
		if (!GetSaveFileNameW(&ofn))
			return;
		sceneFilePath = filename.data();
		if (SaveSceneConfig(sceneFilePath))
			std::cout << "Current scene config saved.\n";
		else
			std::cerr << "Failed to save scene config.\n";
	}

	void MenuBar::SelectRenderer(const RendererType renderer) {
		if (rendererType == renderer)
			return;
		rendererType = renderer;
		rendererDirty = true;
		UpdateRendererMenuCheck();
	}

	void MenuBar::UpdateRendererMenuCheck() const {
		if (!ownerWindow)
			return;
		const HMENU menu = GetMenu(ownerWindow);
		if (!menu)
			return;
		const HMENU rendererMenu = GetSubMenu(menu, 1);
		if (!rendererMenu)
			return;
		int rendererId = kOpenGlRendererId;
		if (rendererType == RendererType::DirectX11)
			rendererId = kDirectX11RendererId;
		else if (rendererType == RendererType::DirectX12)
			rendererId = kDirectX12RendererId;
		else if (rendererType == RendererType::Vulkan)
			rendererId = kVulkanRendererId;
		CheckMenuRadioItem(rendererMenu, kOpenGlRendererId, kVulkanRendererId, rendererId, MF_BYCOMMAND);
		DrawMenuBar(ownerWindow);
	}

	void MenuBar::UpdateLanguageMenuCheck() const {
		if (!ownerWindow)
			return;
		const HMENU menu = GetMenu(ownerWindow);
		if (!menu)
			return;
		const HMENU languageMenu = GetSubMenu(menu, 4);
		if (!languageMenu)
			return;
		int languageId = kEnglishLanguageId;
		if (Language::GetCurrent() == LanguageType::Korean)
			languageId = kKoreanLanguageId;
		else if (Language::GetCurrent() == LanguageType::Japanese)
			languageId = kJapaneseLanguageId;
		else if (Language::GetCurrent() == LanguageType::Chinese)
			languageId = kChineseLanguageId;
		CheckMenuRadioItem(languageMenu, kEnglishLanguageId, kChineseLanguageId, languageId, MF_BYCOMMAND);
		DrawMenuBar(ownerWindow);
	}

	void MenuBar::SelectLanguage(const LanguageType type) {
		if (Language::GetCurrent() == type)
			return;
		Language::SetCurrent(type);
		languageDirty = true;
		UpdateLanguageMenuCheck();
	}

	MenuBar::MenuBar(SceneConfig& config) : sceneConfig(config) {}

	void MenuBar::AddMenu(const HMENU menu) const {
		HMENU fileMenu = CreatePopupMenu();
		AppendMenuW(fileMenu, MF_STRING, kOpenButtonId, Language::Text("menu.open").c_str());
		AppendMenuW(fileMenu, MF_STRING, kSaveButtonId, Language::Text("menu.save").c_str());
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), Language::Text("menu.file").c_str());
		HMENU rendererMenu = CreatePopupMenu();
		AppendMenuW(rendererMenu, MF_STRING, kOpenGlRendererId, L"OpenGL");
		AppendMenuW(rendererMenu, MF_STRING, kDirectX11RendererId, L"DirectX 11");
		AppendMenuW(rendererMenu, MF_STRING, kDirectX12RendererId, L"DirectX 12");
		AppendMenuW(rendererMenu, MF_STRING, kVulkanRendererId, L"Vulkan");
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(rendererMenu), Language::Text("menu.renderer").c_str());
		HMENU physicsMenu = CreatePopupMenu();
		AppendMenuW(physicsMenu, MF_STRING | (physicsEnabled ? MF_CHECKED : MF_UNCHECKED),
			kPhysicsEnabledId, Language::Text("menu.physics_enabled").c_str());
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(physicsMenu), Language::Text("menu.physics").c_str());
		HMENU viewMenu = CreatePopupMenu();
		AppendMenuW(viewMenu, MF_STRING | (fpsVisible ? MF_CHECKED : MF_UNCHECKED),
			kFpsViewId, Language::Text("menu.fps").c_str());
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), Language::Text("menu.view").c_str());
		HMENU languageMenu = CreatePopupMenu();
		AppendMenuW(languageMenu, MF_STRING, kEnglishLanguageId, L"English");
		AppendMenuW(languageMenu, MF_STRING, kKoreanLanguageId, L"한국어");
		AppendMenuW(languageMenu, MF_STRING, kJapaneseLanguageId, L"日本語");
		AppendMenuW(languageMenu, MF_STRING, kChineseLanguageId, L"中文");
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(languageMenu), Language::Text("menu.language").c_str());
		int rendererId = kOpenGlRendererId;
		if (rendererType == RendererType::DirectX11)
			rendererId = kDirectX11RendererId;
		else if (rendererType == RendererType::DirectX12)
			rendererId = kDirectX12RendererId;
		else if (rendererType == RendererType::Vulkan)
			rendererId = kVulkanRendererId;
		CheckMenuRadioItem(rendererMenu, kOpenGlRendererId, kVulkanRendererId, rendererId, MF_BYCOMMAND);
		int languageId = kEnglishLanguageId;
		if (Language::GetCurrent() == LanguageType::Korean)
			languageId = kKoreanLanguageId;
		else if (Language::GetCurrent() == LanguageType::Japanese)
			languageId = kJapaneseLanguageId;
		else if (Language::GetCurrent() == LanguageType::Chinese)
			languageId = kChineseLanguageId;
		CheckMenuRadioItem(languageMenu, kEnglishLanguageId, kChineseLanguageId, languageId, MF_BYCOMMAND);
	}

	bool MenuBar::HandleCommand(const int commandId) {
		switch (commandId) {
			case kOpenButtonId:
				ShowOpenSceneDialog();
				return true;
			case kSaveButtonId:
				ShowSaveSceneDialog();
				return true;
			case kOpenGlRendererId:
				SelectRenderer(RendererType::OpenGL);
				return true;
			case kDirectX11RendererId:
				SelectRenderer(RendererType::DirectX11);
				return true;
			case kDirectX12RendererId:
				SelectRenderer(RendererType::DirectX12);
				return true;
			case kVulkanRendererId:
				SelectRenderer(RendererType::Vulkan);
				return true;
			case kPhysicsEnabledId:
				physicsEnabled = !physicsEnabled;
				if (ownerWindow)
					CheckMenuItem(GetMenu(ownerWindow), kPhysicsEnabledId, MF_BYCOMMAND |
						(physicsEnabled ? MF_CHECKED : MF_UNCHECKED));
				if (ownerWindow)
					DrawMenuBar(ownerWindow);
				return true;
			case kFpsViewId:
				fpsVisible = !fpsVisible;
				if (ownerWindow)
					CheckMenuItem(GetMenu(ownerWindow), kFpsViewId, MF_BYCOMMAND |
						(fpsVisible ? MF_CHECKED : MF_UNCHECKED));
				if (ownerWindow)
					DrawMenuBar(ownerWindow);
				return true;
			case kEnglishLanguageId:
				SelectLanguage(LanguageType::English);
				return true;
			case kKoreanLanguageId:
				SelectLanguage(LanguageType::Korean);
				return true;
			case kJapaneseLanguageId:
				SelectLanguage(LanguageType::Japanese);
				return true;
			case kChineseLanguageId:
				SelectLanguage(LanguageType::Chinese);
				return true;
			default:
				return false;
		}
	}

	void MenuBar::ApplySceneConfig(const SceneConfig& cfg) {
		sceneConfig = cfg;
		sceneFilePath.clear();
		sceneConfigDirty = false;
	}

	void MenuBar::Reset() {
		sceneConfigDirty = false;
		rendererDirty = false;
		languageDirty = false;
	}

	bool MenuBar::ConsumeSceneConfigDirty() {
		const bool dirty = sceneConfigDirty;
		sceneConfigDirty = false;
		return dirty;
	}

	bool MenuBar::ConsumeRendererDirty() {
		const bool dirty = rendererDirty;
		rendererDirty = false;
		return dirty;
	}

	bool MenuBar::ConsumeLanguageDirty() {
		const bool dirty = languageDirty;
		languageDirty = false;
		return dirty;
	}

}
