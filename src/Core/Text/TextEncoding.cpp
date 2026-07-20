#include "Core/Text/TextEncoding.h"

#include <Windows.h>
#include <limits>

namespace Chrivent {
	std::string TextEncoding::WideToUtf8(const std::wstring& wide) {
		if (wide.empty() || wide.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return {};
		const int sourceLength = static_cast<int>(wide.size());
		const int requiredLength = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), sourceLength,
			nullptr, 0, nullptr, nullptr);
		if (requiredLength <= 0)
			return {};
		std::string utf8(requiredLength, '\0');
		const int writtenLength = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), sourceLength,
			utf8.data(), requiredLength, nullptr, nullptr);
		return writtenLength == requiredLength ? utf8 : std::string{};
	}

	std::wstring TextEncoding::Utf8ToWide(const std::string& utf8) {
		if (utf8.empty() || utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return {};
		const int sourceLength = static_cast<int>(utf8.size());
		const int requiredLength = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), sourceLength,
			nullptr, 0);
		if (requiredLength <= 0)
			return {};
		std::wstring wide(requiredLength, L'\0');
		const int writtenLength = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), sourceLength,
			wide.data(), requiredLength);
		return writtenLength == requiredLength ? wide : std::wstring{};
	}

	std::string TextEncoding::ShiftJisToUtf8(const char* shiftJis, const std::size_t size) {
		if (!shiftJis || size == 0)
			return {};
		std::size_t length = 0;
		while (length < size && shiftJis[length] != '\0')
			length++;
		if (length == 0 || length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return {};
		const int sourceLength = static_cast<int>(length);
		const int requiredLength = MultiByteToWideChar(
			932, MB_ERR_INVALID_CHARS, shiftJis, sourceLength,
			nullptr, 0);
		if (requiredLength <= 0)
			return {};
		std::wstring wide(requiredLength, L'\0');
		const int writtenLength = MultiByteToWideChar(
			932, MB_ERR_INVALID_CHARS, shiftJis, sourceLength,
			wide.data(), requiredLength);
		return writtenLength == requiredLength ? WideToUtf8(wide) : std::string{};
	}

	std::filesystem::path TextEncoding::Utf8ToPath(const std::string& utf8) {
		std::u8string encodedPath;
		encodedPath.reserve(utf8.size());
		for (const unsigned char character : utf8)
			encodedPath.push_back(character);
		return std::filesystem::path(encodedPath);
	}
}
