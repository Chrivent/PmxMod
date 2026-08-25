#include "Program/Panel/InformationPanel.h"

#include "Program/Gui/GuiTheme.h"
#include "Program/Language.h"

#include <algorithm>
#include <utility>

namespace Chrivent {
	void InformationPanel::RefreshText() const {
		if (!informationText)
			return;
		std::wstring text;
		for (const auto& [labelKey, value] : fields) {
			if (value.empty())
				continue;
			if (!text.empty())
				text += L"\r\n";
			text += Language::Text(labelKey);
			if (value.find_first_of(L"\r\n") == std::wstring::npos)
				text += L": " + value;
			else
				text += L":\r\n" + value;
		}
		SetWindowTextW(informationText, text.c_str());
	}

	void InformationPanel::Create(const HWND parent) {
		if (informationText)
			return;
		informationText = CreateWindowExW(
			WS_EX_CLIENTEDGE, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
			0, 0, 0, 0, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
		GuiTheme::ApplyControl(informationText);
		RefreshText();
	}

	void InformationPanel::Resize(const RECT& clientRect) {
		if (!informationText)
			return;
		constexpr int margin = 6;
		const int width = std::max(0, static_cast<int>(clientRect.right - clientRect.left) - margin * 2);
		const int height = std::max(0, static_cast<int>(clientRect.bottom - clientRect.top) - margin * 2);
		MoveWindow(informationText, clientRect.left + margin, clientRect.top + margin, width, height, TRUE);
	}

	void InformationPanel::UpdateVisibility(const bool visible) const {
		if (informationText)
			ShowWindow(informationText, visible ? SW_SHOW : SW_HIDE);
	}

	void InformationPanel::UpdateLanguage() {
		RefreshText();
	}

	void InformationPanel::Destroy() {
		if (informationText)
			DestroyWindow(informationText);
		informationText = nullptr;
		fields.clear();
	}

	void InformationPanel::ApplyFields(std::vector<InformationField> informationFields) {
		fields = std::move(informationFields);
		RefreshText();
	}

	void InformationPanel::Clear() {
		fields.clear();
		RefreshText();
	}
}
