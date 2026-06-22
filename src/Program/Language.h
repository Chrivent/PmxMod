#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Chrivent {
	enum class LanguageType {
		English,
		Korean,
		Japanese,
		Chinese
	};

	class Language {
		static inline std::unordered_map<std::string, std::wstring> fallbackTexts;
		static inline std::unordered_map<std::string, std::wstring> currentTexts;
		static inline auto currentType = LanguageType::English;

		// 실행 파일 기준 언어 리소스 디렉터리를 반환한다.
		static std::filesystem::path ResolveLanguageDirectory();
		// Windows UI 표시 언어를 지원 언어 값으로 변환한다.
		static LanguageType DetectWindowsUiLanguage();
		// 지정한 언어의 JSON 파일을 읽어 번역 맵으로 변환한다.
		static std::unordered_map<std::string, std::wstring> LoadFile(const std::filesystem::path& filepath);
		// 언어 값에 대응하는 JSON 파일 이름을 반환한다.
		static std::filesystem::path ResolveFilename(LanguageType type);

	public:
		// Windows UI 표시 언어를 기준으로 번역 리소스를 초기화한다.
		static void Initialize();
		static LanguageType GetCurrent() { return currentType; }
		// 현재 언어를 바꾸고 해당 JSON 번역 리소스를 다시 읽는다.
		static void ChangeCurrent(LanguageType type);
		// 번역 키에 대응하는 문자열을 반환하고 없으면 영어 또는 키를 사용한다.
		static std::wstring Text(const std::string& key);
	};
}
