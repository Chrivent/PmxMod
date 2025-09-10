#include "SjisToUnicode.h"

#include <string>
#include <vector>
#include <cstdint>
#include <windows.h>

namespace saba {
    static std::u32string U16ToU32(const std::u16string_view s) {
        std::u32string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size();) {
            const uint16_t cu = s[i++];
            if (0xD800 <= cu && cu <= 0xDBFF) {
                if (i < s.size()) {
                    const uint16_t cu2 = s[i++];
                    const uint32_t cp = 0x10000 + ((cu - 0xD800) << 10 | cu2 - 0xDC00);
                    out.push_back(static_cast<char32_t>(cp));
                } else {
                    out.push_back(U'\uFFFD');
                }
            } else if (0xDC00 <= cu && cu <= 0xDFFF) {
                out.push_back(U'\uFFFD');
            } else {
                out.push_back(cu);
            }
        }
        return out;
    }

    char16_t ConvertSjisToU16Char(const int ch) {
        char bytes[2];
        int  len = 0;
        if ((ch & 0xFF00) != 0) {
            bytes[0] = static_cast<char>(ch >> 8 & 0xFF);
            bytes[1] = static_cast<char>(ch & 0xFF);
            len = 2;
        } else {
            bytes[0] = static_cast<char>(ch & 0xFF);
            len = 1;
        }
        wchar_t wBuf[2] = {0, 0};
        const int n = MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
                                            bytes, len, wBuf, 2);
        if (n >= 1) return static_cast<char16_t>(wBuf[0]);
        return u'\uFFFD';
    }

    std::u16string ConvertSjisToU16String(const char* sjisCode) {
        if (!sjisCode) return {};
        const int need = MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
                                               sjisCode, -1, nullptr, 0);
        if (need <= 0) return {};
        std::wstring w; w.resize(static_cast<size_t>(need - 1));
        if (need > 1) {
            MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, sjisCode, -1, w.data(), need);
        }
        std::u16string u16; u16.resize(w.size());
        for (size_t i = 0; i < w.size(); ++i) u16[i] = static_cast<char16_t>(w[i]);
        return u16;
    }

    std::u32string ConvertSjisToU32String(const char* sjisCode) {
        return U16ToU32(ConvertSjisToU16String(sjisCode));
    }
}
