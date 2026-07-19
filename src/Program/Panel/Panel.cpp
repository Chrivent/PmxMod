#include "Program/Panel/Panel.h"

namespace Chrivent {
	bool Panel::IsInputLockMessage(const UINT msg) {
		switch (msg) {
		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_MOUSEWHEEL:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CHAR:
			return true;
		default:
			return false;
		}
	}

	LRESULT CALLBACK Panel::InputLockedControlWindowProc(const HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam,
		const UINT_PTR subclassId, const DWORD_PTR data) {
		const auto* panel = reinterpret_cast<Panel*>(data);
		if (msg == WM_NCDESTROY) {
			RemoveWindowSubclass(hwnd, InputLockedControlWindowProc, subclassId);
			return DefSubclassProc(hwnd, msg, wParam, lParam);
		}
		if (panel && panel->inputLocked && IsInputLockMessage(msg))
			return 0;
		return DefSubclassProc(hwnd, msg, wParam, lParam);
	}

	bool Panel::ApplyInputLock(const bool locked) {
		if (inputLocked == locked)
			return false;
		inputLocked = locked;
		return true;
	}
}
