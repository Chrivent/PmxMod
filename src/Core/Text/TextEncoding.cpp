#include "Core/Text/TextEncoding.h"

#include <Windows.h>
#include <limits>
#include <utility>

namespace Chrivent {
	std::expected<std::string, TextEncoding::Error> TextEncoding::TryWideToUtf8(const std::wstring& wide) {
		if (wide.empty())
			return std::string{};
		if (wide.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return std::unexpected(Error::InputTooLarge);
		const int sourceLength = static_cast<int>(wide.size());
		const int requiredLength = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), sourceLength,
			nullptr, 0, nullptr, nullptr);
		if (requiredLength <= 0)
			return std::unexpected(Error::InvalidSequence);
		std::string utf8(requiredLength, '\0');
		const int writtenLength = WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), sourceLength,
			utf8.data(), requiredLength, nullptr, nullptr);
		if (writtenLength != requiredLength)
			return std::unexpected(Error::ConversionFailed);
		return utf8;
	}

	std::expected<std::wstring, TextEncoding::Error> TextEncoding::TryUtf8ToWide(const std::string& utf8) {
		if (utf8.empty())
			return std::wstring{};
		if (utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return std::unexpected(Error::InputTooLarge);
		const int sourceLength = static_cast<int>(utf8.size());
		const int requiredLength = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), sourceLength,
			nullptr, 0);
		if (requiredLength <= 0)
			return std::unexpected(Error::InvalidSequence);
		std::wstring wide(requiredLength, L'\0');
		const int writtenLength = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), sourceLength,
			wide.data(), requiredLength);
		if (writtenLength != requiredLength)
			return std::unexpected(Error::ConversionFailed);
		return wide;
	}

	std::expected<std::string, TextEncoding::Error> TextEncoding::TryShiftJisToUtf8(
		const char* shiftJis, const std::size_t size) {
		if (!shiftJis) {
			if (size == 0)
				return std::string{};
			return std::unexpected(Error::InvalidSequence);
		}
		if (size == 0)
			return std::string{};
		std::size_t length = 0;
		while (length < size && shiftJis[length] != '\0')
			length++;
		if (length == 0)
			return std::string{};
		if (length > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return std::unexpected(Error::InputTooLarge);
		const int sourceLength = static_cast<int>(length);
		const int requiredLength = MultiByteToWideChar(
			932, MB_ERR_INVALID_CHARS, shiftJis, sourceLength,
			nullptr, 0);
		if (requiredLength <= 0)
			return std::unexpected(Error::InvalidSequence);
		std::wstring wide(requiredLength, L'\0');
		const int writtenLength = MultiByteToWideChar(
			932, MB_ERR_INVALID_CHARS, shiftJis, sourceLength,
			wide.data(), requiredLength);
		if (writtenLength != requiredLength)
			return std::unexpected(Error::ConversionFailed);
		return TryWideToUtf8(wide);
	}

	std::expected<std::filesystem::path, TextEncoding::Error> TextEncoding::TryUtf8ToPath(
		const std::string& utf8) {
		auto wide = TryUtf8ToWide(utf8);
		if (!wide)
			return std::unexpected(wide.error());
		return std::filesystem::path(std::move(*wide));
	}
}
