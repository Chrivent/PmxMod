#pragma once

#include <string>
#include <array>
#include <stdexcept>
#include <windows.h>

struct UnicodeUtil {
    static std::string ToUtf8String(const std::wstring& w) {
        if (w.empty())
            return {};
        const int need = ::WideCharToMultiByte(
            CP_UTF8, 0,
            w.c_str(), -1,
            nullptr, 0,
            nullptr, nullptr);
        if (need <= 0)
            return {};
        std::string out(static_cast<size_t>(need), '\0');
        const int written = ::WideCharToMultiByte(
            CP_UTF8, 0,
            w.c_str(), -1,
            out.data(), need,
            nullptr, nullptr);
        if (written <= 0)
            return {};
        if (!out.empty() && out.back() == '\0')
            out.pop_back();
        return out;
    }

    static bool TryToWString(const std::string& utf8, std::wstring& out) {
        out.clear();
        if (utf8.empty())
            return true;
        const int need = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), static_cast<int>(utf8.size()),
            nullptr, 0);
        if (need <= 0)
            return false;
        out.resize(static_cast<size_t>(need));
        return MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), static_cast<int>(utf8.size()),
            out.data(), need) == need;
    }

    static bool ConvU16ToU8(const std::u16string& u16, std::string& out) {
        out.clear();
        if (u16.empty())
            return true;
        const auto w = reinterpret_cast<const wchar_t*>(u16.data());
        const int w_len = static_cast<int>(u16.size());
        const int need = WideCharToMultiByte(
            CP_UTF8, 0,
            w, w_len,
            nullptr, 0,
            nullptr, nullptr);
        if (need <= 0)
            return false;
        out.resize(static_cast<size_t>(need));
        return WideCharToMultiByte(
            CP_UTF8, 0,
            w, w_len,
            out.data(), need,
            nullptr, nullptr) == need;
    }

    static std::u16string ConvertSjisToU16String(const char* sjis) {
        if (!sjis)
            return {};
        const int need = MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, -1,
            nullptr, 0);
        if (need <= 0)
            return {};
        std::wstring w;
        w.resize(static_cast<size_t>(need));
        if (MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, -1,
            w.data(), need) <= 0)
            return {};
        if (!w.empty() && w.back() == L'\0')
            w.pop_back();
        return std::u16string(reinterpret_cast<const char16_t*>(w.data()), w.size());
    }
};
