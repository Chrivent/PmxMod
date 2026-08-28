#include "Program/Panel/CameraPanel.h"

#include "Program/Gui/GuiTheme.h"
#include "Program/Language.h"

#include <algorithm>
#include <utility>

namespace Chrivent {
	void CameraPanel::ShowOpenCameraMotionDialog() {
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
		if (GetOpenFileNameW(&ofn))
			pendingCameraMotionPath = filename.data();
	}

	std::wstring CameraPanel::BuildCameraMotionText() const {
		return cameraMotionPath.empty()
			? Language::Text("camera.motion.none")
			: cameraMotionPath.filename().wstring();
	}

	void CameraPanel::RefreshShaderList() {
		if (!shaderList)
			return;
		updatingShaderList = true;
		ListView_DeleteAllItems(shaderList);
		std::wstring cameraMotionText = BuildCameraMotionText();
		LVITEMW cameraItem{};
		cameraItem.mask = LVIF_TEXT;
		cameraItem.iItem = kCameraMotionRow;
		cameraItem.pszText = cameraMotionText.data();
		SendMessageW(shaderList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&cameraItem));
		for (int index = 0; index < static_cast<int>(shaderNames.size()); index++) {
			LVITEMW item{};
			item.mask = LVIF_TEXT;
			item.iItem = index + kShaderRowOffset;
			item.pszText = const_cast<wchar_t*>(shaderNames[index].c_str());
			SendMessageW(shaderList, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
		}
		ApplyListState();
		updatingShaderList = false;
	}

	void CameraPanel::ApplyListState() {
		if (!shaderList)
			return;
		updatingShaderList = true;
		ListView_SetItemState(shaderList, kCameraMotionRow, INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
		ListView_SetItemState(
			shaderList,
			kCameraMotionRow,
			selectedListIndex == kCameraMotionRow ? LVIS_SELECTED | LVIS_FOCUSED : 0,
			LVIS_SELECTED | LVIS_FOCUSED);
		for (int index = 0; index < static_cast<int>(shaderNames.size()); index++) {
			const int rowIndex = index + kShaderRowOffset;
			const bool checked = index < static_cast<int>(shaderEnabled.size()) && shaderEnabled[index];
			ListView_SetCheckState(shaderList, rowIndex, checked);
			ListView_SetItemState(
				shaderList,
				rowIndex,
				rowIndex == selectedListIndex ? LVIS_SELECTED | LVIS_FOCUSED : 0,
				LVIS_SELECTED | LVIS_FOCUSED);
		}
		updatingShaderList = false;
	}

	void CameraPanel::DrawMotionButton(const NMLVCUSTOMDRAW& customDraw) const {
		if (!shaderList || customDraw.nmcd.dwItemSpec != kCameraMotionRow || customDraw.iSubItem != kMotionColumn)
			return;
		RECT buttonRect{};
		ListView_GetSubItemRect(shaderList, kCameraMotionRow, kMotionColumn, LVIR_BOUNDS, &buttonRect);
		InflateRect(&buttonRect, -4, -3);
		const COLORREF fillColor = playing ? GuiTheme::disabledControlColor : RGB(47, 53, 64);
		const COLORREF borderColor = playing ? RGB(72, 78, 90) : GuiTheme::borderColor;
		const HBRUSH fillBrush = CreateSolidBrush(fillColor);
		FillRect(customDraw.nmcd.hdc, &buttonRect, fillBrush);
		DeleteObject(fillBrush);
		const HPEN borderPen = CreatePen(PS_SOLID, 1, borderColor);
		const HGDIOBJ previousPen = SelectObject(customDraw.nmcd.hdc, borderPen);
		const HGDIOBJ previousBrush = SelectObject(customDraw.nmcd.hdc, GetStockObject(NULL_BRUSH));
		Rectangle(customDraw.nmcd.hdc, buttonRect.left, buttonRect.top, buttonRect.right, buttonRect.bottom);
		SelectObject(customDraw.nmcd.hdc, previousBrush);
		SelectObject(customDraw.nmcd.hdc, previousPen);
		DeleteObject(borderPen);
		SetBkMode(customDraw.nmcd.hdc, TRANSPARENT);
		SetTextColor(customDraw.nmcd.hdc, playing ? GuiTheme::disabledTextColor : GuiTheme::textColor);
		DrawTextW(customDraw.nmcd.hdc, Language::Text("camera.motion").c_str(), -1, &buttonRect,
			DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	}

	void CameraPanel::QueueShaderSelection(const int shaderIndex, const bool enabled) {
		if (shaderIndex < 0 || shaderIndex >= static_cast<int>(shaderNames.size()))
			return;
		selectedShaderIndex = shaderIndex;
		selectedListIndex = shaderIndex + kShaderRowOffset;
		if (shaderIndex >= static_cast<int>(shaderEnabled.size()))
			shaderEnabled.resize(shaderNames.size(), false);
		shaderEnabled[shaderIndex] = enabled;
		pendingSelectedShaderIndex = selectedShaderIndex;
		pendingShaderEffectEnabled = enabled;
		ApplyListState();
	}

	void CameraPanel::QueueBuiltInShaderToggle(const BuiltInShaderToggle shader, const bool enabled) {
		pendingBuiltInShaderToggle = shader;
		pendingBuiltInShaderEnabled = enabled;
		switch (shader) {
			case BuiltInShaderToggle::Model:
				modelShaderEnabled = enabled;
				break;
			case BuiltInShaderToggle::Edge:
				edgeShaderEnabled = enabled;
				break;
			case BuiltInShaderToggle::GroundShadow:
				groundShadowShaderEnabled = enabled;
				break;
		}
	}

	void CameraPanel::QueueCameraMotionSelection() {
		selectedListIndex = kCameraMotionRow;
		pendingCameraMotionSelected = true;
		ApplyListState();
	}

	void CameraPanel::Create(const HWND parent) {
		if (shaderList)
			return;
		parentWindow = parent;
		modelShaderCheck = CreateWindowExW(0, L"BUTTON", Language::Text("camera.shader.model").c_str(),
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, parent,
			reinterpret_cast<HMENU>(modelShaderCheckId), GetModuleHandleW(nullptr), nullptr);
		edgeShaderCheck = CreateWindowExW(0, L"BUTTON", Language::Text("camera.shader.edge").c_str(),
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, parent,
			reinterpret_cast<HMENU>(edgeShaderCheckId), GetModuleHandleW(nullptr), nullptr);
		groundShadowShaderCheck = CreateWindowExW(0, L"BUTTON",
			Language::Text("camera.shader.ground_shadow").c_str(),
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, parent,
			reinterpret_cast<HMENU>(groundShadowShaderCheckId), GetModuleHandleW(nullptr), nullptr);
		shaderList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER,
			0, 0, 0, 0,
			parent, reinterpret_cast<HMENU>(shaderListId), GetModuleHandleW(nullptr), nullptr);
		ListView_SetExtendedListViewStyle(shaderList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		LVCOLUMNW column{};
		column.mask = LVCF_WIDTH;
		ListView_InsertColumn(shaderList, 0, &column);
		LVCOLUMNW motionColumn{};
		motionColumn.mask = LVCF_WIDTH;
		ListView_InsertColumn(shaderList, kMotionColumn, &motionColumn);
		GuiTheme::ApplyControl(modelShaderCheck);
		GuiTheme::ApplyControl(edgeShaderCheck);
		GuiTheme::ApplyControl(groundShadowShaderCheck);
		GuiTheme::ApplyControl(shaderList);
		UpdateBuiltInShaderStates(modelShaderEnabled, edgeShaderEnabled, groundShadowShaderEnabled);
		RefreshShaderList();
	}

	void CameraPanel::Resize(const RECT& clientRect) {
		if (!shaderList)
			return;
		constexpr int margin = 12;
		constexpr int checkHeight = 22;
		constexpr int gap = 8;
		const int width = std::max(0, static_cast<int>(clientRect.right - clientRect.left - margin * 2));
		const int shaderTop = clientRect.top + margin;
		const int listTop = shaderTop + checkHeight + gap;
		const int listHeight = std::max(0, static_cast<int>(clientRect.bottom - listTop - margin));
		const int checkWidth = std::max(0, width / 3);
		MoveWindow(modelShaderCheck, clientRect.left + margin, shaderTop, checkWidth, checkHeight, TRUE);
		MoveWindow(edgeShaderCheck, clientRect.left + margin + checkWidth, shaderTop, checkWidth, checkHeight, TRUE);
		MoveWindow(groundShadowShaderCheck, clientRect.left + margin + checkWidth * 2,
			shaderTop, width - checkWidth * 2, checkHeight, TRUE);
		MoveWindow(shaderList, clientRect.left + margin, listTop, width, listHeight, TRUE);
		const int motionColumnWidth = std::min(82, std::max(0, width / 3));
		ListView_SetColumnWidth(shaderList, 0, std::max(0, width - motionColumnWidth - 4));
		ListView_SetColumnWidth(shaderList, kMotionColumn, motionColumnWidth);
	}

	void CameraPanel::UpdateVisibility(const bool visible) const {
		if (modelShaderCheck)
			ShowWindow(modelShaderCheck, visible ? SW_SHOW : SW_HIDE);
		if (edgeShaderCheck)
			ShowWindow(edgeShaderCheck, visible ? SW_SHOW : SW_HIDE);
		if (groundShadowShaderCheck)
			ShowWindow(groundShadowShaderCheck, visible ? SW_SHOW : SW_HIDE);
		if (shaderList)
			ShowWindow(shaderList, visible ? SW_SHOW : SW_HIDE);
	}

	void CameraPanel::ApplyPlaybackState(const bool isPlaying) {
		if (playing == isPlaying)
			return;
		playing = isPlaying;
		if (shaderList)
			InvalidateRect(shaderList, nullptr, FALSE);
	}

	void CameraPanel::UpdateLanguage() {
		if (modelShaderCheck)
			SetWindowTextW(modelShaderCheck, Language::Text("camera.shader.model").c_str());
		if (edgeShaderCheck)
			SetWindowTextW(edgeShaderCheck, Language::Text("camera.shader.edge").c_str());
		if (groundShadowShaderCheck)
			SetWindowTextW(groundShadowShaderCheck, Language::Text("camera.shader.ground_shadow").c_str());
		RefreshShaderList();
	}

	bool CameraPanel::HandleCommand(const UINT_PTR commandId, const int notificationCode) {
		if (notificationCode == BN_CLICKED && commandId == modelShaderCheckId) {
			QueueBuiltInShaderToggle(BuiltInShaderToggle::Model,
				SendMessageW(modelShaderCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
			return true;
		}
		if (notificationCode == BN_CLICKED && commandId == edgeShaderCheckId) {
			QueueBuiltInShaderToggle(BuiltInShaderToggle::Edge,
				SendMessageW(edgeShaderCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
			return true;
		}
		if (notificationCode == BN_CLICKED && commandId == groundShadowShaderCheckId) {
			QueueBuiltInShaderToggle(BuiltInShaderToggle::GroundShadow,
				SendMessageW(groundShadowShaderCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
			return true;
		}
		return false;
	}

	bool CameraPanel::HandleNotify(const NMHDR& notifyHeader, LRESULT& result) {
		result = 0;
		if (!shaderList || notifyHeader.hwndFrom != shaderList || notifyHeader.idFrom != shaderListId)
			return false;
		if (updatingShaderList)
			return true;
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
				&& customDraw.nmcd.dwItemSpec == kCameraMotionRow && customDraw.iSubItem == kMotionColumn) {
				DrawMotionButton(customDraw);
				result = CDRF_SKIPDEFAULT;
				return true;
			}
			return false;
		}
		if (notifyHeader.code == LVN_ITEMCHANGED) {
			const auto& change = reinterpret_cast<const NMLISTVIEW&>(notifyHeader);
			if (change.iItem < 0)
				return true;
			const bool selectionChanged = (change.uChanged & LVIF_STATE) != 0
				&& ((change.uOldState ^ change.uNewState) & LVIS_SELECTED) != 0
				&& (change.uNewState & LVIS_SELECTED) != 0;
			const bool checkChanged = (change.uChanged & LVIF_STATE) != 0
				&& ((change.uOldState ^ change.uNewState) & LVIS_STATEIMAGEMASK) != 0;
			if (!selectionChanged && !checkChanged)
				return true;
			if (change.iItem == kCameraMotionRow) {
				ListView_SetItemState(shaderList, kCameraMotionRow, INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
				if (selectionChanged)
					QueueCameraMotionSelection();
				return true;
			}
			const int shaderIndex = change.iItem - kShaderRowOffset;
			const bool checked = ListView_GetCheckState(shaderList, change.iItem);
			QueueShaderSelection(shaderIndex, checked);
			return true;
		}
		if (notifyHeader.code == NM_CLICK) {
			const auto& click = reinterpret_cast<const NMITEMACTIVATE&>(notifyHeader);
			if (click.iItem == kCameraMotionRow && click.iSubItem == kMotionColumn) {
				QueueCameraMotionSelection();
				if (!playing)
					ShowOpenCameraMotionDialog();
				return true;
			}
		}
		return false;
	}

	void CameraPanel::Destroy() {
		if (groundShadowShaderCheck)
			DestroyWindow(groundShadowShaderCheck);
		if (edgeShaderCheck)
			DestroyWindow(edgeShaderCheck);
		if (modelShaderCheck)
			DestroyWindow(modelShaderCheck);
		if (shaderList)
			DestroyWindow(shaderList);
		parentWindow = nullptr;
		modelShaderCheck = nullptr;
		edgeShaderCheck = nullptr;
		groundShadowShaderCheck = nullptr;
		shaderList = nullptr;
		pendingCameraMotionPath.clear();
		pendingCameraMotionSelected = false;
		pendingSelectedShaderIndex = -1;
		pendingBuiltInShaderToggle.reset();
		pendingShaderEffectEnabled = false;
		shaderEnabled.clear();
		playing = false;
	}

	bool CameraPanel::ConsumeBuiltInShaderToggle(BuiltInShaderToggle& shader, bool& enabled) {
		if (!pendingBuiltInShaderToggle.has_value())
			return false;
		shader = *pendingBuiltInShaderToggle;
		enabled = pendingBuiltInShaderEnabled;
		pendingBuiltInShaderToggle.reset();
		return true;
	}

	bool CameraPanel::ConsumeSelectedShaderIndex(size_t& shaderIndex, bool& enabled) {
		if (pendingSelectedShaderIndex < 0)
			return false;
		shaderIndex = pendingSelectedShaderIndex;
		enabled = pendingShaderEffectEnabled;
		pendingSelectedShaderIndex = -1;
		return true;
	}

	bool CameraPanel::ConsumeCameraMotionSelected() {
		if (!pendingCameraMotionSelected)
			return false;
		pendingCameraMotionSelected = false;
		return true;
	}

	bool CameraPanel::ConsumeCameraMotionPath(std::filesystem::path& motionPath) {
		if (pendingCameraMotionPath.empty())
			return false;
		motionPath = std::move(pendingCameraMotionPath);
		pendingCameraMotionPath.clear();
		return true;
	}

	void CameraPanel::UpdateCameraMotionPath(const std::filesystem::path& motionPath) {
		cameraMotionPath = motionPath;
		RefreshShaderList();
	}

	void CameraPanel::UpdateShaderNames(const std::vector<std::wstring>& names, const size_t selectedIndex, const std::vector<bool>& enabledStates) {
		shaderNames = names;
		shaderEnabled = enabledStates;
		shaderEnabled.resize(shaderNames.size(), false);
		if (shaderNames.empty()) {
			selectedShaderIndex = -1;
			pendingSelectedShaderIndex = -1;
			pendingShaderEffectEnabled = false;
		} else
			selectedShaderIndex = static_cast<int>(std::min(selectedIndex, shaderNames.size() - 1));
		if (selectedListIndex >= static_cast<int>(shaderNames.size()) + kShaderRowOffset)
			selectedListIndex = kCameraMotionRow;
		RefreshShaderList();
	}

	void CameraPanel::UpdateBuiltInShaderStates(const bool modelEnabled, const bool edgeEnabled,
		const bool groundShadowEnabled) {
		modelShaderEnabled = modelEnabled;
		edgeShaderEnabled = edgeEnabled;
		groundShadowShaderEnabled = groundShadowEnabled;
		if (modelShaderCheck)
			SendMessageW(modelShaderCheck, BM_SETCHECK, modelShaderEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
		if (edgeShaderCheck)
			SendMessageW(edgeShaderCheck, BM_SETCHECK, edgeShaderEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
		if (groundShadowShaderCheck) {
			SendMessageW(groundShadowShaderCheck, BM_SETCHECK,
				groundShadowShaderEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
		}
	}
}
