#pragma once

#include <windows.h>

namespace Chrivent {
	class Panel {
	public:
		virtual ~Panel() = default;

		virtual void AddMenu(HMENU menu) {}
		virtual void Create(HWND parent) {}
		virtual void Resize(const RECT& clientRect) {}
		virtual bool HandleCommand(int commandId) { return false; }
		virtual bool HandleScroll(HWND control) { return false; }
		virtual void Destroy() {}
	};
}
