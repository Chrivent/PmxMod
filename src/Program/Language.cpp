#include "Program/Language.h"

#include "Util.h"

#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include <windows.h>

namespace Chrivent {
	std::filesystem::path Language::ResolveLanguageDirectory() {
		std::vector<wchar_t> path(MAX_PATH);
		while (true) {
			const DWORD length = GetModuleFileNameW(nullptr, path.data(), path.size());
			if (length < path.size() - 1)
				return std::filesystem::path(std::wstring(path.data(), length)).parent_path()
					/ "resource" / "language";
			path.resize(path.size() * 2);
		}
	}

	LanguageType Language::DetectWindowsUiLanguage() {
		switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
			case LANG_KOREAN:
				return LanguageType::Korean;
			case LANG_JAPANESE:
				return LanguageType::Japanese;
			case LANG_CHINESE:
				return LanguageType::Chinese;
			default:
				return LanguageType::English;
		}
	}

	std::unordered_map<std::string, std::wstring> Language::LoadFile(const std::filesystem::path& filepath) {
		std::ifstream stream(filepath, std::ios::binary);
		if (!stream)
			return {};
		const auto json = nlohmann::json::parse(stream, nullptr, false);
		if (!json.is_object())
			return {};
		std::unordered_map<std::string, std::wstring> texts;
		texts.reserve(json.size());
		for (const auto& [key, value] : json.items()) {
			if (value.is_string())
				texts.emplace(key, Util::Utf8ToWString(value.get<std::string>()));
		}
		return texts;
	}

	std::filesystem::path Language::ResolveFilename(const LanguageType type) {
		switch (type) {
			case LanguageType::Korean:
				return L"ko.json";
			case LanguageType::Japanese:
				return L"ja.json";
			case LanguageType::Chinese:
				return L"zh.json";
			case LanguageType::English:
			default:
				return L"en.json";
		}
	}

	void Language::Initialize() {
		const auto directory = ResolveLanguageDirectory();
		fallbackTexts = LoadFile(directory / ResolveFilename(LanguageType::English));
		ChangeCurrent(DetectWindowsUiLanguage());
	}

	void Language::ChangeCurrent(const LanguageType type) {
		currentType = type;
		currentTexts = LoadFile(ResolveLanguageDirectory() / ResolveFilename(type));
	}

	std::wstring Language::Text(const std::string& key) {
		const std::string ownedKey(key);
		if (const auto current = currentTexts.find(ownedKey); current != currentTexts.end())
			return current->second;
		if (const auto fallback = fallbackTexts.find(ownedKey); fallback != fallbackTexts.end())
			return fallback->second;
		return Util::Utf8ToWString(ownedKey);
	}
}
