#pragma once

#include <string>

namespace saba
{
	char16_t ConvertSjisToU16Char(int ch);
	std::u16string ConvertSjisToU16String(const char* sjisCode);
	std::u32string ConvertSjisToU32String(const char* sjisCode);
}

