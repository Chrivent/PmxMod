#pragma once

#include <filesystem>
#include <string>

namespace Chrivent {
	// Windows 문자열, UTF-8, Shift-JIS와 파일 경로 사이의 인코딩을 변환한다.
	class TextEncoding {
	public:
		// Windows wide 문자열을 UTF-8 문자열로 변환한다.
		static std::string WideToUtf8(const std::wstring& wide);
		// UTF-8 문자열을 Windows wide 문자열로 변환한다.
		static std::wstring Utf8ToWide(const std::string& utf8);
		// 고정 길이 Shift-JIS 바이트열을 UTF-8 문자열로 변환한다.
		static std::string ShiftJisToUtf8(const char* shiftJis, std::size_t size);
		// UTF-8 문자열을 파일 시스템 경로로 변환한다.
		static std::filesystem::path Utf8ToPath(const std::string& utf8);
	};
}
