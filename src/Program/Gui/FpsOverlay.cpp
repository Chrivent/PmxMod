#include "Program/Gui/FpsOverlay.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <format>

namespace Chrivent {
	LRESULT CALLBACK FpsOverlay::WindowProc(const HWND window, const UINT message,
		const WPARAM wParam, const LPARAM lParam) {
		if (message == WM_ERASEBKGND)
			return 1;
		if (message == WM_PAINT) {
			PAINTSTRUCT paint{};
			const HDC deviceContext = BeginPaint(window, &paint);
			RECT client{};
			GetClientRect(window, &client);
			const HBRUSH transparentColor = CreateSolidBrush(RGB(0, 0, 0));
			FillRect(deviceContext, &client, transparentColor);
			DeleteObject(transparentColor);
			wchar_t text[32]{};
			GetWindowTextW(window, text, 32);
			SetBkMode(deviceContext, TRANSPARENT);
			SetTextColor(deviceContext, RGB(160, 255, 120));
			const auto windowFont = reinterpret_cast<HFONT>(GetWindowLongPtrW(window, GWLP_USERDATA));
			const HGDIOBJ previousFont = windowFont ? SelectObject(deviceContext, windowFont) : nullptr;
			DrawTextW(deviceContext, text, -1, &client, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
			if (previousFont)
				SelectObject(deviceContext, previousFont);
			EndPaint(window, &paint);
			return 0;
		}
		return DefWindowProcW(window, message, wParam, lParam);
	}

	void FpsOverlay::Position() const {
		if (overlayWindow == nullptr || ownerWindow == nullptr)
			return;
		const HWND viewerWindow = glfwGetWin32Window(ownerWindow);
		if (viewerWindow == nullptr)
			return;
		POINT origin{ 12, 12 };
		ClientToScreen(viewerWindow, &origin);
		SetWindowPos(overlayWindow, HWND_TOP, origin.x, origin.y, 120, 32,
			SWP_NOACTIVATE | SWP_SHOWWINDOW);
	}

	FpsOverlay::~FpsOverlay() {
		Reset();
	}

	void FpsOverlay::Create(GLFWwindow* sourceWindow) {
		Reset();
		if (sourceWindow == nullptr)
			return;
		const HWND viewerWindow = glfwGetWin32Window(sourceWindow);
		if (viewerWindow == nullptr)
			return;
		const HINSTANCE instance = GetModuleHandleW(nullptr);
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = L"PmxModFpsOverlay";
		RegisterClassExW(&windowClass);
		font = CreateFontW(-22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, L"Arial");
		overlayWindow = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
			windowClass.lpszClassName, L"0 FPS", WS_POPUP, 0, 0, 120, 32,
			viewerWindow, nullptr, instance, nullptr);
		if (overlayWindow == nullptr) {
			Reset();
			return;
		}
		ownerWindow = sourceWindow;
		SetWindowLongPtrW(overlayWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(font));
		SetLayeredWindowAttributes(overlayWindow, RGB(0, 0, 0), 255, LWA_COLORKEY);
		Position();
	}

	void FpsOverlay::Reset() {
		if (overlayWindow)
			DestroyWindow(overlayWindow);
		if (font)
			DeleteObject(font);
		ownerWindow = nullptr;
		overlayWindow = nullptr;
		font = nullptr;
	}

	void FpsOverlay::Update(const double fps) const {
		if (overlayWindow == nullptr)
			return;
		const std::wstring text = std::format(L"{:.1f} FPS", fps);
		SetWindowTextW(overlayWindow, text.c_str());
		InvalidateRect(overlayWindow, nullptr, FALSE);
	}

	void FpsOverlay::SetVisible(const bool visible) const {
		if (overlayWindow == nullptr)
			return;
		if (visible) {
			Position();
			ShowWindow(overlayWindow, SW_SHOWNOACTIVATE);
		} else
			ShowWindow(overlayWindow, SW_HIDE);
	}
}
