#include "Viewer.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <format>

#define	STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Chrivent {
    Viewer::Viewer() : info(std::make_unique<ViewerInfo>()) {}

    Viewer::~Viewer() {
        if (fpsOverlay)
            DestroyWindow(fpsOverlay);
        if (fpsFont)
            DeleteObject(fpsFont);
    }

    LRESULT CALLBACK Viewer::FpsOverlayWindowProc(
        const HWND hwnd,
        const UINT msg,
        const WPARAM wParam,
        const LPARAM lParam) {
        if (msg == WM_PAINT) {
            PAINTSTRUCT paint{};
            const HDC deviceContext = BeginPaint(hwnd, &paint);
            RECT client{};
            GetClientRect(hwnd, &client);
            const HBRUSH background = CreateSolidBrush(RGB(24, 27, 32));
            FillRect(deviceContext, &client, background);
            DeleteObject(background);
            wchar_t text[32]{};
            GetWindowTextW(hwnd, text, std::size(text));
            SetBkMode(deviceContext, TRANSPARENT);
            SetTextColor(deviceContext, RGB(240, 243, 247));
            const auto font = reinterpret_cast<HFONT>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            const HGDIOBJ previousFont = font ? SelectObject(deviceContext, font) : nullptr;
            DrawTextW(deviceContext, text, -1, &client, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            if (previousFont)
                SelectObject(deviceContext, previousFont);
            EndPaint(hwnd, &paint);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    unsigned char* Viewer::LoadImageRgba(const std::filesystem::path& texturePath, int& x, int& y, int& comp) {
        x = y = comp = 0;
        FILE* imageFile = nullptr;
        if (_wfopen_s(&imageFile, texturePath.c_str(), L"rb") != 0 || !imageFile)
            return nullptr;
        stbi_uc* image = stbi_load_from_file(imageFile, &x, &y, &comp, STBI_rgb_alpha);
        std::fclose(imageFile);
        return image;
    }

    void Viewer::InitDirs(const std::filesystem::path& shaderSubDir) {
        std::vector<wchar_t> buf(MAX_PATH);
        while (true) {
            const DWORD n = GetModuleFileNameW(nullptr, buf.data(), buf.size());
            if (n < buf.size() - 1) {
                resourceDir = std::filesystem::path(std::wstring(buf.data(), n));
                break;
            }
            buf.resize(buf.size() * 2);
        }
        resourceDir = resourceDir.parent_path() / "resource";
        info->shaderDir = resourceDir / shaderSubDir;
        info->pmxDir = resourceDir / "mmd";
    }

    void Viewer::CreateFpsOverlay() {
        if (fpsOverlay || !info->window)
            return;
        const HWND viewerWindow = glfwGetWin32Window(info->window);
        if (!viewerWindow)
            return;
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = FpsOverlayWindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = L"PmxModFpsOverlay";
        RegisterClassExW(&windowClass);
        fpsFont = CreateFontW(
            -18, 0, 0, 0, FW_SEMIBOLD,
            FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI");
        fpsOverlay = CreateWindowExW(
            0, windowClass.lpszClassName, L"0 FPS",
            WS_CHILD,
            12, 12, 100, 28,
            viewerWindow, nullptr, instance, nullptr);
        SetWindowLongPtrW(fpsOverlay, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(fpsFont));
    }

    void Viewer::SetFpsVisible(const bool visible) const {
        if (fpsOverlay)
            ShowWindow(fpsOverlay, visible ? SW_SHOWNA : SW_HIDE);
    }

    void Viewer::UpdateFps(const double fps) const {
        if (!fpsOverlay)
            return;
        const std::wstring text = std::format(L"{:.1f} FPS", fps);
        SetWindowTextW(fpsOverlay, text.c_str());
        InvalidateRect(fpsOverlay, nullptr, TRUE);
    }
}
