#include "ModelPanel.h"

#include <algorithm>

namespace Chrivent {
	void ModelPanel::ShowOpenModelDialog() {
		std::vector filename(32768, L'\0');
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = parentWindow;
		ofn.lpstrFilter = L"PMX Model (*.pmx)\0*.pmx\0All Files (*.*)\0*.*\0";
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = static_cast<DWORD>(filename.size());
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pmx";
		if (GetOpenFileNameW(&ofn))
			pendingModelPath = filename.data();
	}

	void ModelPanel::RefreshModelList() const {
		if (!modelList)
			return;
		SendMessageW(modelList, LB_RESETCONTENT, 0, 0);
		for (const auto& modelPath : modelPaths) {
			const std::wstring name = modelPath.filename().wstring();
			SendMessageW(modelList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
		}
	}

	void ModelPanel::Create(const HWND parent) {
		if (addButton || modelList)
			return;
		parentWindow = parent;
		addButton = CreateWindowExW(
			0, L"BUTTON", L"Add",
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(addButtonId)), GetModuleHandleW(nullptr), nullptr);
		modelList = CreateWindowExW(
			WS_EX_CLIENTEDGE, L"LISTBOX", L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(modelListId)), GetModuleHandleW(nullptr), nullptr);
		RefreshModelList();
	}

	void ModelPanel::Resize(const RECT& clientRect) {
		if (!addButton || !modelList)
			return;
		constexpr int margin = 12;
		constexpr int buttonWidth = 72;
		constexpr int buttonHeight = 28;
		constexpr int gap = 8;
		const int width = (std::max)(0, static_cast<int>(clientRect.right - clientRect.left - margin * 2));
		const int listHeight = (std::max)(0, static_cast<int>(
			clientRect.bottom - clientRect.top - margin * 2 - buttonHeight - gap));
		MoveWindow(addButton, clientRect.right - margin - buttonWidth, clientRect.top + margin,
			buttonWidth, buttonHeight, TRUE);
		MoveWindow(modelList, clientRect.left + margin, clientRect.top + margin + buttonHeight + gap,
			width, listHeight, TRUE);
	}

	bool ModelPanel::HandleCommand(const int commandId, const int notificationCode) {
		if (commandId == addButtonId) {
			ShowOpenModelDialog();
			return true;
		}
		if (commandId != modelListId || notificationCode != LBN_SELCHANGE || !modelList)
			return false;
		const auto selection = SendMessageW(modelList, LB_GETCURSEL, 0, 0);
		if (selection == LB_ERR)
			return true;
		selectedModelIndex = selection;
		pendingSelectedModelIndex = selectedModelIndex;
		return true;
	}

	void ModelPanel::Destroy() {
		if (addButton)
			DestroyWindow(addButton);
		if (modelList)
			DestroyWindow(modelList);
		parentWindow = nullptr;
		addButton = nullptr;
		modelList = nullptr;
		pendingModelPath.clear();
		selectedModelIndex = -1;
		pendingSelectedModelIndex = -1;
	}

	bool ModelPanel::ConsumeAddModelPath(std::filesystem::path& modelPath) {
		if (pendingModelPath.empty())
			return false;
		modelPath = std::move(pendingModelPath);
		pendingModelPath.clear();
		return true;
	}

	bool ModelPanel::ConsumeSelectedModelIndex(size_t& modelIndex) {
		if (pendingSelectedModelIndex < 0)
			return false;
		modelIndex = pendingSelectedModelIndex;
		pendingSelectedModelIndex = -1;
		return true;
	}

	void ModelPanel::UpdateModelPaths(const std::vector<std::filesystem::path>& paths) {
		modelPaths = paths;
		RefreshModelList();
		if (modelPaths.empty()) {
			selectedModelIndex = -1;
			pendingSelectedModelIndex = -1;
			return;
		}
		selectedModelIndex = std::clamp(selectedModelIndex, 0, static_cast<int>(modelPaths.size() - 1));
		SendMessageW(modelList, LB_SETCURSEL, selectedModelIndex, 0);
		pendingSelectedModelIndex = selectedModelIndex;
	}
}
