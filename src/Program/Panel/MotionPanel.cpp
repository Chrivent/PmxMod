#include "MotionPanel.h"

#include <algorithm>

namespace Chrivent {
	void MotionPanel::Create(const HWND parent) {
		if (titleText)
			return;
		titleText = CreateWindowExW(
			0, L"STATIC", L"Motion",
			WS_CHILD | WS_VISIBLE | SS_LEFT,
			0, 0, 0, 0,
			parent, nullptr, GetModuleHandleW(nullptr), nullptr);
	}

	void MotionPanel::Resize(const RECT& clientRect) {
		if (!titleText)
			return;
		constexpr int margin = 12;
		const int width = (std::max)(0, static_cast<int>(clientRect.right - clientRect.left - margin * 2));
		MoveWindow(titleText, clientRect.left + margin, clientRect.top + margin, width, 24, TRUE);
	}

	void MotionPanel::Destroy() {
		if (titleText)
			DestroyWindow(titleText);
		titleText = nullptr;
	}
}
