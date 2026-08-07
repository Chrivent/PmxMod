#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace Chrivent {
	// Windows 문자열, UTF-8, Shift-JIS와 파일 경로 사이의 인코딩을 변환한다.
	class TextEncoding {
	public:
		// 문자열 변환 실패 원인을 분류한다.
		enum class Error {
			InputTooLarge,
			InvalidSequence,
			ConversionFailed
		};

		// Windows wide 문자열을 UTF-8 문자열로 변환하고 실패 원인을 반환한다.
		static std::expected<std::string, Error> TryWideToUtf8(const std::wstring& wide);
		// UTF-8 문자열을 Windows wide 문자열로 변환하고 실패 원인을 반환한다.
		static std::expected<std::wstring, Error> TryUtf8ToWide(const std::string& utf8);
		// 고정 길이 Shift-JIS 바이트열을 UTF-8 문자열로 변환하고 실패 원인을 반환한다.
		static std::expected<std::string, Error> TryShiftJisToUtf8(const char* shiftJis, std::size_t size);
		// UTF-8 문자열을 파일 시스템 경로로 변환하고 실패 원인을 반환한다.
		static std::expected<std::filesystem::path, Error> TryUtf8ToPath(const std::string& utf8);
		// Windows wide 문자열 변환이 실패하면 빈 UTF-8 문자열을 반환한다.
		static std::string WideToUtf8OrEmpty(const std::wstring& wide) {
			return TryWideToUtf8(wide).value_or(std::string{});
		}
		// UTF-8 문자열 변환이 실패하면 빈 Windows wide 문자열을 반환한다.
		static std::wstring Utf8ToWideOrEmpty(const std::string& utf8) {
			return TryUtf8ToWide(utf8).value_or(std::wstring{});
		}
	};
}
