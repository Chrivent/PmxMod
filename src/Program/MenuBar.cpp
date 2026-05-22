#include "MenuBar.h"

#include <iostream>

namespace Chrivent {
	MenuBar::MenuBar(SceneConfig& config) : sceneConfig(config) {}

	void MenuBar::AddMenu(const HMENU menu) const {
		HMENU fileMenu = CreatePopupMenu();
		AppendMenuW(fileMenu, MF_STRING, kOpenButtonId, L"Open...");
		AppendMenuW(fileMenu, MF_STRING, kSaveButtonId, L"Save...");
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");

		HMENU rendererMenu = CreatePopupMenu();
		AppendMenuW(rendererMenu, MF_STRING, kOpenGlRendererId, L"OpenGL");
		AppendMenuW(rendererMenu, MF_STRING, kDirectX11RendererId, L"DirectX 11");
		AppendMenuW(rendererMenu, MF_GRAYED, kVulkanRendererId, L"Vulkan");
		AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(rendererMenu), L"Renderer");
		int rendererId = kOpenGlRendererId;
		if (rendererType == RendererType::DirectX11)
			rendererId = kDirectX11RendererId;
		else if (rendererType == RendererType::Vulkan)
			rendererId = kVulkanRendererId;
		CheckMenuRadioItem(rendererMenu, kOpenGlRendererId, kVulkanRendererId, rendererId, MF_BYCOMMAND);
	}

	void MenuBar::AttachOwner(const HWND owner) {
		ownerWindow = owner;
	}

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
			std::cerr << "Failed to load scene config.\n";
	}

	void MenuBar::ShowSaveSceneDialog() {
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
		int rendererId = kOpenGlRendererId;
		if (rendererType == RendererType::DirectX11)
			rendererId = kDirectX11RendererId;
		else if (rendererType == RendererType::Vulkan)
			rendererId = kVulkanRendererId;
		CheckMenuRadioItem(menu, kOpenGlRendererId, kVulkanRendererId, rendererId, MF_BYCOMMAND);
		DrawMenuBar(ownerWindow);
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
				SelectRenderer(RendererType::OpenGl);
				return true;
			case kDirectX11RendererId:
				SelectRenderer(RendererType::DirectX11);
				return true;
			case kVulkanRendererId:
				SelectRenderer(RendererType::Vulkan);
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
}
