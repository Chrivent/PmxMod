#include "Util.h"

#include <Windows.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Chrivent {
    glm::mat4 Util::InvZ(const glm::mat4& m) {
        const glm::mat4 invZ = glm::scale(glm::mat4(1), glm::vec3(1, 1, -1));
        return invZ * m * invZ;
    }

    std::string Util::WStringToUtf8(const std::wstring& w) {
        std::string utf8;
        if (w.empty())
            return utf8;
        const int need = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), w.size(),
            nullptr, 0, nullptr, nullptr);
        if (need <= 0)
            return utf8;
        utf8.resize(need);
        const int written = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), w.size(),
            utf8.data(), need, nullptr, nullptr);
        if (written != need)
            utf8.clear();
        return utf8;
    }

    std::string Util::SjisToUtf8(const char* sjis) {
        std::string result;
        if (!sjis)
            return result;
        const int need = MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, -1,
            nullptr, 0);
        if (need <= 0)
            return result;
        std::wstring w(need, L'\0');
        const int written = MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, -1,
            w.data(), need);
        if (written <= 0)
            return result;
        if (!w.empty() && w.back() == L'\0')
            w.pop_back();
        result = WStringToUtf8(w);
        return result;
    }
    
    std::filesystem::path Util::PathFromUtf8(const std::string& utf8) {
        std::u8string u8;
        u8.reserve(utf8.size());
        for (const unsigned char c : utf8)
            u8.push_back(c);
        return std::filesystem::path(u8);
    }
}
