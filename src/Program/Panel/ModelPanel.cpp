#include "Program/Panel/ModelPanel.h"

#include "Program/Gui/GuiTheme.h"
#include "Program/Language.h"

#include <algorithm>
#include <CommCtrl.h>
#include <utility>

namespace Chrivent {
	void ModelPanel::ShowOpenModelDialog() {
		std::vector filename(32768, L'\0');
		std::wstring filter = Language::Text("model.dialog.pmx");
		filter.append(1, L'\0');
		filter += L"*.pmx";
		filter.append(1, L'\0');
		filter += Language::Text("dialog.all_files");
		filter.append(1, L'\0');
		filter += L"*.*";
		filter.append(2, L'\0');
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = parentWindow;
		ofn.lpstrFilter = filter.c_str();
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = static_cast<DWORD>(filename.size());
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"pmx";
		if (GetOpenFileNameW(&ofn))
			pendingModelPath = filename.data();
	}

	void ModelPanel::ShowOpenModelMotionDialog(const int modelIndex) {
		if (modelIndex < 0 || modelIndex >= static_cast<int>(modelPaths.size()))
			return;
		std::vector filename(32768, L'\0');
		std::wstring filter = Language::Text("camera.dialog.vmd");
		filter.append(1, L'\0');
		filter += L"*.vmd";
		filter.append(1, L'\0');
		filter += Language::Text("dialog.all_files");
		filter.append(1, L'\0');
		filter += L"*.*";
		filter.append(2, L'\0');
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = parentWindow;
		ofn.lpstrFilter = filter.c_str();
		ofn.lpstrFile = filename.data();
		ofn.nMaxFile = static_cast<DWORD>(filename.size());
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"vmd";
		if (GetOpenFileNameW(&ofn)) {
			pendingModelMotionIndex = modelIndex;
			pendingModelMotionPath = filename.data();
		}
	}

	void ModelPanel::RefreshModelList() const {
		if (!modelList)
			return;
		ListView_DeleteAllItems(modelList);
		for (int index = 0; index < static_cast<int>(modelPaths.size()); index++) {
			const auto& modelPath = modelPaths[index];
			const std::wstring name = modelPath.filename().wstring();
			LVITEMW item{};
			item.mask = LVIF_TEXT;
			item.iItem = index;
			item.pszText = const_cast<wchar_t*>(name.c_str());
			SendMessageW(modelList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
			const std::wstring motionText = Language::Text("model.motion");
			LVITEMW motionItem{};
			motionItem.iSubItem = kMotionColumn;
			motionItem.pszText = const_cast<wchar_t*>(motionText.c_str());
			SendMessageW(modelList, LVM_SETITEMTEXTW, index, reinterpret_cast<LPARAM>(&motionItem));
		}
		ApplyModelSelection();
	}

	void ModelPanel::ApplyModelSelection() const {
		if (!modelList)
			return;
		for (int index = 0; index < static_cast<int>(modelPaths.size()); index++)
			ListView_SetItemState(modelList, index, index == selectedModelIndex ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
	}

	void ModelPanel::DrawMotionButton(const NMLVCUSTOMDRAW& customDraw) const {
		if (!modelList || customDraw.iSubItem != kMotionColumn)
			return;
		RECT buttonRect{};
		ListView_GetSubItemRect(modelList, static_cast<int>(customDraw.nmcd.dwItemSpec), kMotionColumn, LVIR_BOUNDS, &buttonRect);
		InflateRect(&buttonRect, -4, -3);
		DrawFrameControl(customDraw.nmcd.hdc, &buttonRect, DFC_BUTTON, DFCS_BUTTONPUSH);
		SetBkMode(customDraw.nmcd.hdc, TRANSPARENT);
		DrawTextW(customDraw.nmcd.hdc, Language::Text("model.motion").c_str(), -1, &buttonRect,
			DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	}

	void ModelPanel::Create(const HWND parent) {
		if (addButton || modelList)
			return;
		parentWindow = parent;
		deleteButton = CreateWindowExW(
			0, L"BUTTON", Language::Text("model.delete").c_str(),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(deleteButtonId), GetModuleHandleW(nullptr), nullptr);
		addButton = CreateWindowExW(
			0, L"BUTTON", Language::Text("model.add").c_str(),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(addButtonId), GetModuleHandleW(nullptr), nullptr);
		modelList = CreateWindowExW(
			WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(modelListId), GetModuleHandleW(nullptr), nullptr);
		ListView_SetExtendedListViewStyle(modelList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		ListView_SetBkColor(modelList, GuiTheme::controlColor);
		ListView_SetTextBkColor(modelList, GuiTheme::controlColor);
		ListView_SetTextColor(modelList, GuiTheme::textColor);
		LVCOLUMNW modelColumn{};
		modelColumn.mask = LVCF_WIDTH;
		ListView_InsertColumn(modelList, 0, &modelColumn);
		LVCOLUMNW motionColumn{};
		motionColumn.mask = LVCF_WIDTH;
		ListView_InsertColumn(modelList, kMotionColumn, &motionColumn);
		GuiTheme::ApplyControl(deleteButton);
		GuiTheme::ApplyControl(addButton);
		GuiTheme::ApplyControl(modelList);
		RefreshModelList();
	}

	void ModelPanel::Resize(const RECT& clientRect) {
		if (!addButton || !modelList)
			return;
		constexpr int margin = 12;
		constexpr int buttonWidth = 72;
		constexpr int buttonHeight = 28;
		constexpr int gap = 8;
		const int width = std::max(0, static_cast<int>(clientRect.right - clientRect.left - margin * 2));
		const int listHeight = std::max(0, static_cast<int>(
			clientRect.bottom - clientRect.top - margin * 2 - buttonHeight - gap));
		const int deleteX = clientRect.right - margin - buttonWidth * 2 - gap;
		const int addX = clientRect.right - margin - buttonWidth;
		MoveWindow(deleteButton, deleteX, clientRect.top + margin, buttonWidth, buttonHeight, TRUE);
		MoveWindow(addButton, addX, clientRect.top + margin,
			buttonWidth, buttonHeight, TRUE);
		MoveWindow(modelList, clientRect.left + margin, clientRect.top + margin + buttonHeight + gap,
			width, listHeight, TRUE);
		const int motionColumnWidth = std::min(82, std::max(0, width / 3));
		ListView_SetColumnWidth(modelList, 0, std::max(0, width - motionColumnWidth - 4));
		ListView_SetColumnWidth(modelList, kMotionColumn, motionColumnWidth);
	}

	void ModelPanel::UpdateLanguage() {
		if (deleteButton)
			SetWindowTextW(deleteButton, Language::Text("model.delete").c_str());
		if (addButton)
			SetWindowTextW(addButton, Language::Text("model.add").c_str());
		RefreshModelList();
	}

	void ModelPanel::UpdateVisibility(const bool visible) const {
		if (deleteButton)
			ShowWindow(deleteButton, visible ? SW_SHOW : SW_HIDE);
		if (addButton)
			ShowWindow(addButton, visible ? SW_SHOW : SW_HIDE);
		if (modelList)
			ShowWindow(modelList, visible ? SW_SHOW : SW_HIDE);
	}

	bool ModelPanel::HandleCommand(const UINT_PTR commandId, const int notificationCode) {
		if (commandId == addButtonId) {
			ShowOpenModelDialog();
			return true;
		}
		if (commandId == deleteButtonId) {
			if (selectedModelIndex >= 0)
				pendingDeleteModelIndex = selectedModelIndex;
			return true;
		}
		return false;
	}

	bool ModelPanel::HandleNotify(const NMHDR& notifyHeader, LRESULT& result) {
		result = 0;
		if (!modelList || notifyHeader.hwndFrom != modelList || notifyHeader.idFrom != modelListId)
			return false;
		if (notifyHeader.code == NM_CUSTOMDRAW) {
			const auto& customDraw = reinterpret_cast<const NMLVCUSTOMDRAW&>(notifyHeader);
			if (customDraw.nmcd.dwDrawStage == CDDS_PREPAINT) {
				result = CDRF_NOTIFYITEMDRAW;
				return true;
			}
			if (customDraw.nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
				result = CDRF_NOTIFYSUBITEMDRAW;
				return true;
			}
			if (customDraw.nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)
				&& customDraw.iSubItem == kMotionColumn) {
				DrawMotionButton(customDraw);
				result = CDRF_SKIPDEFAULT;
				return true;
			}
			return false;
		}
		if (notifyHeader.code == LVN_ITEMCHANGED) {
			const auto& change = reinterpret_cast<const NMLISTVIEW&>(notifyHeader);
			if (change.iItem < 0 || (change.uChanged & LVIF_STATE) == 0)
				return true;
			if (((change.uOldState ^ change.uNewState) & LVIS_SELECTED) == 0 || (change.uNewState & LVIS_SELECTED) == 0)
				return true;
			selectedModelIndex = change.iItem;
			pendingSelectedModelIndex = selectedModelIndex;
			return true;
		}
		if (notifyHeader.code == NM_CLICK) {
			const auto& click = reinterpret_cast<const NMITEMACTIVATE&>(notifyHeader);
			if (click.iItem >= 0 && click.iSubItem == kMotionColumn) {
				selectedModelIndex = click.iItem;
				pendingSelectedModelIndex = selectedModelIndex;
				ApplyModelSelection();
				ShowOpenModelMotionDialog(click.iItem);
				return true;
			}
		}
		return false;
	}

	void ModelPanel::Destroy() {
		if (deleteButton)
			DestroyWindow(deleteButton);
		if (addButton)
			DestroyWindow(addButton);
		if (modelList)
			DestroyWindow(modelList);
		parentWindow = nullptr;
		deleteButton = nullptr;
		addButton = nullptr;
		modelList = nullptr;
		pendingModelPath.clear();
		pendingModelMotionPath.clear();
		selectedModelIndex = -1;
		pendingSelectedModelIndex = -1;
		pendingDeleteModelIndex = -1;
		pendingModelMotionIndex = -1;
	}

	bool ModelPanel::ConsumeAddModelPath(std::filesystem::path& modelPath) {
		if (pendingModelPath.empty())
			return false;
		modelPath = std::move(pendingModelPath);
		pendingModelPath.clear();
		return true;
	}

	bool ModelPanel::ConsumeDeleteModelIndex(size_t& modelIndex) {
		if (pendingDeleteModelIndex < 0)
			return false;
		modelIndex = pendingDeleteModelIndex;
		pendingDeleteModelIndex = -1;
		return true;
	}

	bool ModelPanel::ConsumeModelMotionPath(size_t& modelIndex, std::filesystem::path& motionPath) {
		if (pendingModelMotionIndex < 0 || pendingModelMotionPath.empty())
			return false;
		modelIndex = pendingModelMotionIndex;
		motionPath = std::move(pendingModelMotionPath);
		pendingModelMotionIndex = -1;
		pendingModelMotionPath.clear();
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
			pendingDeleteModelIndex = -1;
			pendingModelMotionIndex = -1;
			pendingModelMotionPath.clear();
			return;
		}
		selectedModelIndex = std::clamp(selectedModelIndex, 0, static_cast<int>(modelPaths.size() - 1));
		ApplyModelSelection();
		pendingSelectedModelIndex = selectedModelIndex;
	}
}
