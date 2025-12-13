#pragma once

#include <vector>
#include <algorithm>
#include <Windows.h>

#include "UnicodeUtil.h"

struct PathUtil {
	static std::string GetExecutablePath() {
		std::vector<wchar_t> modulePath(MAX_PATH);
		if (GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size())) == 0)
			return "";
		return UnicodeUtil::ToUtf8String(modulePath.data());
	}

	static std::string Combine(const std::string & a, const std::string & b) {
		std::string result;
		for (const auto& part: { a, b }) {
			if (!part.empty()) {
				const auto pos = part.find_last_not_of("\\/");
				if (pos != std::string::npos) {
					constexpr char PathDelimiter = '\\';
					if (!result.empty())
						result.append(&PathDelimiter, 1);
					result.append(part.c_str(), pos + 1);
				}
			}
		}
		return result;
	}

	static std::string GetDirectoryName(const std::string & path) {
		const auto pos = path.find_last_of("\\/");
		if (pos == std::string::npos)
			return "";
		return path.substr(0, pos);
	}

	static std::string GetFilename(const std::string & path) {
		const auto pos = path.find_last_of("\\/");
		if (pos == std::string::npos)
			return path;
		return path.substr(pos + 1, path.size() - pos);
	}

	static std::string GetExt(const std::string & path) {
		const auto pos = path.find_last_of('.');
		if (pos == std::string::npos)
			return "";
		std::string ext = path.substr(pos + 1, path.size() - pos);
		for (auto &ch: ext)
			ch = static_cast<char>(tolower(ch));
		return ext;
	}

	static std::string Normalize(const std::string & path) {
		std::string result = path;
		std::ranges::replace(result, '/', '\\');
		return result;
	}
};
