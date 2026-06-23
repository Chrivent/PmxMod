#include "Program/Panel/CameraPanel.h"

#include "Program/Gui/GuiTheme.h"

#include <algorithm>

namespace Chrivent {
	void CameraPanel::RefreshShaderList() const {
		if (!shaderList)
			return;
		SendMessageW(shaderList, LB_RESETCONTENT, 0, 0);
		for (const auto& shaderName : shaderNames)
			SendMessageW(shaderList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(shaderName.c_str()));
		if (selectedShaderIndex >= 0)
			SendMessageW(shaderList, LB_SETCURSEL, selectedShaderIndex, 0);
	}

	void CameraPanel::Create(const HWND parent) {
		if (shaderList)
			return;
		shaderList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY, 0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(shaderListId), GetModuleHandleW(nullptr), nullptr);
		GuiTheme::ApplyControl(shaderList);
		RefreshShaderList();
	}

	void CameraPanel::Resize(const RECT& clientRect) {
		if (!shaderList)
			return;
		constexpr int margin = 12;
		const int width = std::max(0, static_cast<int>(clientRect.right - clientRect.left - margin * 2));
		const int height = std::max(0, static_cast<int>(clientRect.bottom - clientRect.top - margin * 2));
		MoveWindow(shaderList, clientRect.left + margin, clientRect.top + margin, width, height, TRUE);
	}

	void CameraPanel::UpdateVisibility(const bool visible) const {
		if (shaderList)
			ShowWindow(shaderList, visible ? SW_SHOW : SW_HIDE);
	}

	bool CameraPanel::HandleCommand(const UINT_PTR commandId, const int notificationCode) {
		if (commandId != shaderListId || notificationCode != LBN_SELCHANGE || !shaderList)
			return false;
		const auto selection = SendMessageW(shaderList, LB_GETCURSEL, 0, 0);
		if (selection == LB_ERR)
			return true;
		selectedShaderIndex = selection;
		pendingSelectedShaderIndex = selectedShaderIndex;
		return true;
	}

	void CameraPanel::Destroy() {
		if (shaderList)
			DestroyWindow(shaderList);
		shaderList = nullptr;
		pendingSelectedShaderIndex = -1;
	}

	bool CameraPanel::ConsumeSelectedShaderIndex(size_t& shaderIndex) {
		if (pendingSelectedShaderIndex < 0)
			return false;
		shaderIndex = pendingSelectedShaderIndex;
		pendingSelectedShaderIndex = -1;
		return true;
	}

	void CameraPanel::UpdateShaderNames(const std::vector<std::wstring>& names, const size_t selectedIndex) {
		shaderNames = names;
		if (shaderNames.empty()) {
			selectedShaderIndex = -1;
			pendingSelectedShaderIndex = -1;
		} else
			selectedShaderIndex = std::min(selectedIndex, shaderNames.size() - 1);
		RefreshShaderList();
	}
}
