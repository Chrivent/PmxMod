#pragma once

#include "UnicodeUtil.h"
#include "File.h"

#include <string>

namespace saba
{
	template <size_t Size>
	struct MMDFileString
	{
		MMDFileString() {
			for (auto &ch: m_buffer) {
				ch = '\0';
			}
		}

		std::string ToString() const { return std::string(m_buffer); }
		std::string ToUtf8String() const;

		char	m_buffer[Size + 1]{};
	};

	template <size_t Size>
	bool Read(MMDFileString<Size>* str, File& file) {
		return file.Read(str->m_buffer, Size);
	}

	template<size_t Size>
	std::string MMDFileString<Size>::ToUtf8String() const {
		const std::u16string u16Str = saba::ConvertSjisToU16String(m_buffer);
		std::string u8Str;
		ConvU16ToU8(u16Str, u8Str);
		return u8Str;
	}
}
