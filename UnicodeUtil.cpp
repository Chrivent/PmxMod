#include "UnicodeUtil.h"

#include <stdexcept>
#include <windows.h>

namespace saba
{
	std::string ToUtf8String(const std::wstring & wStr)
	{
		std::string utf8Str;
		if (!TryToUtf8String(wStr, utf8Str))
			throw std::invalid_argument("Failed to convert UTF-8 string.");
		return utf8Str;
	}

	bool TryToWString(const std::string & utf8Str, std::wstring & wStr)
	{
		if (sizeof(wchar_t) == sizeof(char16_t))
		{
			std::u16string utf16Str;
			if (!ConvU8ToU16(utf8Str, utf16Str))
				return false;
			wStr = reinterpret_cast<const wchar_t*>(utf16Str.c_str());
		}

		return true;
	}

	bool TryToUtf8String(const std::wstring & wStr, std::string & utf8Str)
	{
		if (sizeof(wchar_t) == sizeof(char16_t))
		{
			const auto utf16Str = reinterpret_cast<const char16_t*>(wStr.c_str());
			if (!ConvU16ToU8(utf16Str, utf8Str))
				return false;
		}

		return true;
	}

	namespace {
		int GetU8ByteCount(const char ch) {
			if (static_cast<uint8_t>(ch) < 0x80)
				return 1;
			if (0xC2 <= static_cast<uint8_t>(ch) && static_cast<uint8_t>(ch) < 0xE0)
				return 2;
			if (0xE0 <= static_cast<uint8_t>(ch) && static_cast<uint8_t>(ch) < 0xF0)
				return 3;
			if (0xF0 <= static_cast<uint8_t>(ch) && static_cast<uint8_t>(ch) < 0xF8)
				return 4;
			return 0;
		}

		bool IsU8LaterByte(const char ch) {
			return 0x80 <= static_cast<uint8_t>(ch) && static_cast<uint8_t>(ch) < 0xC0;
		}

		bool IsU16HighSurrogate(const char16_t ch) { return 0xD800 <= ch && ch < 0xDC00; }

		bool IsU16LowSurrogate(const char16_t ch) { return 0xDC00 <= ch && ch < 0xE000; }
	}  // namespace

	bool ConvChU8ToU16(const std::array<char, 4>& u8Ch, std::array<char16_t, 2>& u16Ch) {
		char32_t u32Ch;
		if (!ConvChU8ToU32(u8Ch, u32Ch))
			return false;
		if (!ConvChU32ToU16(u32Ch, u16Ch))
			return false;
		return true;
	}

	bool ConvChU8ToU32(const std::array<char, 4>& u8Ch, char32_t& u32Ch) {
		const int numBytes = GetU8ByteCount(u8Ch[0]);
		if (numBytes == 0) {
			return false;
		}
		switch (numBytes) {
		case 1:
			u32Ch = static_cast<char32_t>(u8Ch[0]);
			break;
		case 2:
			if (!IsU8LaterByte(u8Ch[1])) {
				return false;
			}
			if ((static_cast<uint8_t>(u8Ch[0]) & 0x1E) == 0) {
				return false;
			}

			u32Ch = static_cast<char32_t>(u8Ch[0] & 0x1F) << 6;
			u32Ch |= static_cast<char32_t>(u8Ch[1] & 0x3F);
			break;
		case 3:
			if (!IsU8LaterByte(u8Ch[1]) || !IsU8LaterByte(u8Ch[2])) {
				return false;
			}
			if ((static_cast<uint8_t>(u8Ch[0]) & 0x0F) == 0 &&
				(static_cast<uint8_t>(u8Ch[1]) & 0x20) == 0) {
				return false;
			}

			u32Ch = static_cast<char32_t>(u8Ch[0] & 0x0F) << 12;
			u32Ch |= static_cast<char32_t>(u8Ch[1] & 0x3F) << 6;
			u32Ch |= static_cast<char32_t>(u8Ch[2] & 0x3F);
			break;
		case 4:
			if (!IsU8LaterByte(u8Ch[1]) || !IsU8LaterByte(u8Ch[2]) ||
				!IsU8LaterByte(u8Ch[3])) {
				return false;
			}
			if ((static_cast<uint8_t>(u8Ch[0]) & 0x07) == 0 &&
				(static_cast<uint8_t>(u8Ch[1]) & 0x30) == 0) {
				return false;
			}

			u32Ch = static_cast<char32_t>(u8Ch[0] & 0x07) << 18;
			u32Ch |= static_cast<char32_t>(u8Ch[1] & 0x3F) << 12;
			u32Ch |= static_cast<char32_t>(u8Ch[2] & 0x3F) << 6;
			u32Ch |= static_cast<char32_t>(u8Ch[3] & 0x3F);
			break;
		default: ;
		}

		return true;
	}

	bool ConvChU16ToU8(const std::array<char16_t, 2>& u16Ch,
		std::array<char, 4>& u8Ch) {
		char32_t u32Ch;
		if (!ConvChU16ToU32(u16Ch, u32Ch)) {
			return false;
		}
		if (!ConvChU32ToU8(u32Ch, u8Ch)) {
			return false;
		}
		return true;
	}

	bool ConvChU16ToU32(const std::array<char16_t, 2>& u16Ch, char32_t& u32Ch) {
		if (IsU16HighSurrogate(u16Ch[0])) {
			if (IsU16LowSurrogate(u16Ch[1])) {
				u32Ch = 0x10000 + (char32_t(u16Ch[0]) - 0xD800) * 0x400 +
					(char32_t(u16Ch[1]) - 0xDC00);
			}
			else if (u16Ch[1] == 0) {
				u32Ch = u16Ch[0];
			}
			else {
				return false;
			}
		}
		else if (IsU16LowSurrogate(u16Ch[0])) {
			if (u16Ch[1] == 0) {
				u32Ch = u16Ch[0];
			}
			else {
				return false;
			}
		}
		else {
			u32Ch = u16Ch[0];
		}

		return true;
	}

	bool ConvChU32ToU8(const char32_t u32Ch, std::array<char, 4>& u8Ch) {
		if (u32Ch > 0x10FFFF) {
			return false;
		}

		if (u32Ch < 128) {
			u8Ch[0] = char(u32Ch);
			u8Ch[1] = 0;
			u8Ch[2] = 0;
			u8Ch[3] = 0;
		}
		else if (u32Ch < 2048) {
			u8Ch[0] = 0xC0 | char(u32Ch >> 6);
			u8Ch[1] = 0x80 | (char(u32Ch) & 0x3F);
			u8Ch[2] = 0;
			u8Ch[3] = 0;
		}
		else if (u32Ch < 65536) {
			u8Ch[0] = 0xE0 | char(u32Ch >> 12);
			u8Ch[1] = 0x80 | (char(u32Ch >> 6) & 0x3F);
			u8Ch[2] = 0x80 | (char(u32Ch) & 0x3F);
			u8Ch[3] = 0;
		}
		else {
			u8Ch[0] = 0xF0 | char(u32Ch >> 18);
			u8Ch[1] = 0x80 | (char(u32Ch >> 12) & 0x3F);
			u8Ch[2] = 0x80 | (char(u32Ch >> 6) & 0x3F);
			u8Ch[3] = 0x80 | (char(u32Ch) & 0x3F);
		}

		return true;
	}

	bool ConvChU32ToU16(const char32_t u32Ch, std::array<char16_t, 2>& u16Ch) {
		if (u32Ch > 0x10FFFF) {
			return false;
		}

		if (u32Ch < 0x10000) {
			u16Ch[0] = char16_t(u32Ch);
			u16Ch[1] = 0;
		}
		else {
			u16Ch[0] = char16_t((u32Ch - 0x10000) / 0x400 + 0xD800);
			u16Ch[1] = char16_t((u32Ch - 0x10000) % 0x400 + 0xDC00);
		}

		return true;
	}

	bool ConvU8ToU16(const std::string& u8Str, std::u16string& u16Str) {
		for (auto u8It = u8Str.begin(); u8It != u8Str.end(); ++u8It) {
			auto numBytes = GetU8ByteCount((*u8It));
			if (numBytes == 0) {
				return false;
			}

			std::array<char, 4> u8Ch;
			u8Ch[0] = (*u8It);
			for (int i = 1; i < numBytes; i++) {
				++u8It;
				if (u8It == u8Str.end()) {
					return false;
				}
				u8Ch[i] = (*u8It);
			}

			std::array<char16_t, 2> u16Ch;
			if (!ConvChU8ToU16(u8Ch, u16Ch)) {
				return false;
			}

			u16Str.push_back(u16Ch[0]);
			if (u16Ch[1] != 0) {
				u16Str.push_back(u16Ch[1]);
			}
		}
		return true;
	}

	bool ConvU16ToU8(const std::u16string& u16Str, std::string& u8Str) {
		for (auto u16It = u16Str.begin(); u16It != u16Str.end(); ++u16It) {
			std::array<char16_t, 2> u16Ch;
			if (IsU16HighSurrogate((*u16It))) {
				u16Ch[0] = (*u16It);
				++u16It;
				if (u16It == u16Str.end()) {
					return false;
				}
				u16Ch[1] = (*u16It);
			}
			else {
				u16Ch[0] = (*u16It);
				u16Ch[1] = 0;
			}

			std::array<char, 4> u8Ch;
			if (!ConvChU16ToU8(u16Ch, u8Ch)) {
				return false;
			}
			if (u8Ch[0] != 0) {
				u8Str.push_back(u8Ch[0]);
			}
			if (u8Ch[1] != 0) {
				u8Str.push_back(u8Ch[1]);
			}
			if (u8Ch[2] != 0) {
				u8Str.push_back(u8Ch[2]);
			}
			if (u8Ch[3] != 0) {
				u8Str.push_back(u8Ch[3]);
			}
		}
		return true;
	}

	bool ConvU16ToU32(const std::u16string& u16Str, std::u32string& u32Str) {
		for (auto u16It = u16Str.begin(); u16It != u16Str.end(); ++u16It) {
			std::array<char16_t, 2> u16Ch;
			if (IsU16HighSurrogate((*u16It))) {
				u16Ch[0] = (*u16It);
				++u16It;
				if (u16It == u16Str.end()) {
					return false;
				}
				u16Ch[1] = (*u16It);
			}
			else {
				u16Ch[0] = (*u16It);
				u16Ch[1] = 0;
			}

			char32_t u32Ch;
			if (!ConvChU16ToU32(u16Ch, u32Ch)) {
				return false;
			}
			u32Str.push_back(u32Ch);
		}
		return true;
	}

	bool ConvU32ToU8(const std::u32string& u32Str, std::string& u8Str) {
		for (auto u32It = u32Str.begin(); u32It != u32Str.end(); ++u32It) {
			std::array<char, 4> u8Ch;
			if (!ConvChU32ToU8((*u32It), u8Ch)) {
				return false;
			}

			if (u8Ch[0] != 0) {
				u8Str.push_back(u8Ch[0]);
			}
			if (u8Ch[1] != 0) {
				u8Str.push_back(u8Ch[1]);
			}

			if (u8Ch[2] != 0) {
				u8Str.push_back(u8Ch[2]);
			}
			if (u8Ch[3] != 0) {
				u8Str.push_back(u8Ch[3]);
			}
		}
		return true;
	}

	bool ConvU32ToU16(const std::u32string& u32Str, std::u16string& u16Str) {
		for (auto u32It = u32Str.begin(); u32It != u32Str.end(); ++u32It) {
			std::array<char16_t, 2> u16Ch;
			if (!ConvChU32ToU16((*u32It), u16Ch)) {
				return false;
			}

			if (u16Ch[0] != 0) {
				u16Str.push_back(u16Ch[0]);
			}
			if (u16Ch[1] != 0) {
				u16Str.push_back(u16Ch[1]);
			}
		}
		return true;
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
		std::u32string u32Str;
		ConvU16ToU32(ConvertSjisToU16String(sjisCode), u32Str);
		return u32Str;
	}
}
