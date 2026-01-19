#pragma once

#include <windows.h>

struct Util {
    static glm::mat4 InvZ(const glm::mat4& m) {
        const glm::mat4 invZ = glm::scale(glm::mat4(1), glm::vec3(1, 1, -1));
        return invZ * m * invZ;
    }

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
};
