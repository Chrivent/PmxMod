#include "Util.h"

#include <Windows.h>
#include <limits>
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
        if (w.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return utf8;
        const int sourceLength = static_cast<int>(w.size());
        const int need = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), sourceLength,
            nullptr, 0, nullptr, nullptr);
        if (need <= 0)
            return utf8;
        utf8.resize(need);
        const int written = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), sourceLength,
            utf8.data(), need, nullptr, nullptr);
        if (written != need)
            utf8.clear();
        return utf8;
    }

    std::wstring Util::Utf8ToWString(const std::string& utf8) {
        if (utf8.empty())
            return {};
        if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return {};
        const int sourceLength = static_cast<int>(utf8.size());
        const int need = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), sourceLength,
            nullptr, 0);
        if (need <= 0)
            return {};
        std::wstring result(need, L'\0');
        const int written = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), sourceLength,
            result.data(), need);
        if (written != need)
            return {};
        return result;
    }

    std::string Util::SjisToUtf8(const char* sjis, const std::size_t size) {
        std::string result;
        if (!sjis || size == 0)
            return result;
        std::size_t length = 0;
        while (length < size && sjis[length] != '\0')
            length++;
        if (length == 0 || length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return result;
        const int sourceLength = static_cast<int>(length);
        const int need = MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, sourceLength,
            nullptr, 0);
        if (need <= 0)
            return result;
        std::wstring w(need, L'\0');
        const int written = MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, sourceLength,
            w.data(), need);
        if (written != need)
            return result;
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
