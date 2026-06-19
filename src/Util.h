#pragma once

#include <string>
#include <filesystem>
#include <utility>
#include <glm/glm.hpp>

namespace Chrivent {
    class Util {
    public:
        // enum 플래그에 지정한 비트가 포함되어 있는지 확인한다.
        template <typename Enum>
        static constexpr bool HasFlag(Enum flags, Enum flag) {
            return (std::to_underlying(flags) & std::to_underlying(flag)) != 0;
        }
        
        // Z축 반전 좌표계 사이에서 행렬을 변환한다.
        static glm::mat4 InvZ(const glm::mat4& m);
        // Windows wide 문자열을 UTF-8 문자열로 변환한다.
        static std::string WStringToUtf8(const std::wstring& w);
        // Shift-JIS C 문자열을 UTF-8 문자열로 변환한다.
        static std::string SjisToUtf8(const char* sjis);
        static std::filesystem::path Util::PathFromUtf8(const std::string& utf8);
    };
}
