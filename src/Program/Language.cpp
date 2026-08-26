#include "Program/Language.h"

#include "Core/Text/TextEncoding.h"

#include <fstream>
#include <limits>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>
#include <windows.h>

namespace Chrivent {
	std::filesystem::path Language::ResolveLanguageDirectory() {
		std::vector<wchar_t> path(MAX_PATH);
		while (true) {
			if (path.size() > std::numeric_limits<DWORD>::max())
				return {};
			const DWORD capacity = static_cast<DWORD>(path.size());
			const DWORD length = GetModuleFileNameW(nullptr, path.data(), capacity);
			if (length == 0)
				return {};
			if (length < capacity)
				return std::filesystem::path(std::wstring(path.data(), length)).parent_path()
					/ "resource" / "language";
			if (path.size() > std::numeric_limits<DWORD>::max() / 2)
				return {};
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
			if (!value.is_string())
				continue;
			if (auto text = TextEncoding::TryUtf8ToWide(value.get<std::string>()))
				texts.emplace(key, std::move(*text));
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
		if (const auto current = currentTexts.find(key); current != currentTexts.end())
			return current->second;
		if (const auto fallback = fallbackTexts.find(key); fallback != fallbackTexts.end())
			return fallback->second;
		return TextEncoding::Utf8ToWideOrEmpty(key);
	}
}
