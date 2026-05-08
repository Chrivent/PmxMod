#pragma once

#include <windows.h>

class Util {
public:
    /// Z축 반전 좌표계 사이에서 행렬을 변환한다.
    static glm::mat4 InvZ(const glm::mat4& m) {
        const glm::mat4 invZ = glm::scale(glm::mat4(1), glm::vec3(1, 1, -1));
        return invZ * m * invZ;
    }

    /// Windows wide 문자열을 UTF-8 문자열로 변환한다.
    static std::string WStringToUtf8(const std::wstring& w) {
        if (w.empty())
            return {};
        const int need = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), static_cast<int>(w.size()),
            nullptr, 0, nullptr, nullptr);
        if (need <= 0)
            return {};
        std::string utf8(static_cast<size_t>(need), '\0');
        const int written = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), static_cast<int>(w.size()),
            utf8.data(), need, nullptr, nullptr);
        if (written != need)
            return {};
        return utf8;
    }

    /// Shift-JIS C 문자열을 UTF-8 문자열로 변환한다.
    static std::string SjisToUtf8(const char* sjis) {
        if (!sjis)
            return {};
        const int need = MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, -1,
            nullptr, 0);
        if (need <= 0)
            return {};
        std::wstring w(static_cast<size_t>(need), L'\0');
        const int written = MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, -1,
            w.data(), need);
        if (written <= 0)
            return {};
        if (!w.empty() && w.back() == L'\0')
            w.pop_back();
        return WStringToUtf8(w);
    }
};
