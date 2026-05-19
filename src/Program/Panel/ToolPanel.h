#pragma once

#include <windows.h>

namespace Chrivent {
	class ToolPanel {
	public:
		virtual ~ToolPanel() = default;

		virtual void AddMenu(HMENU menu) {}
		virtual void Create(HWND parent) {}
		virtual void Resize(const RECT& clientRect) {}
		virtual bool HandleCommand(int commandId) { return false; }
		virtual void Destroy() {}
	};
}
