#include "Path.h"

#include <algorithm>
#include <Windows.h>

#include "UnicodeUtil.h"

namespace saba
{
	namespace
	{
		constexpr char PathDelimiter = '\\';
		auto PathDelimiters = "\\/";
	}

	std::string PathUtil::GetCWD() {
		const DWORD sz = GetCurrentDirectoryW(0, nullptr);
		std::vector<wchar_t> buffer(sz);
		GetCurrentDirectory(sz, reinterpret_cast<LPSTR>(&buffer[0]));
		std::string workDir = ToUtf8String(&buffer[0]);
		return workDir;
	}

	std::string PathUtil::GetExecutablePath() {
		std::vector<wchar_t> modulePath(MAX_PATH);
		if (GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size())) == 0)
			return "";
		return ToUtf8String(modulePath.data());
	}

	std::string PathUtil::Combine(const std::vector<std::string>& parts) {
		std::string result;
		for (const auto& part: parts) {
			if (!part.empty()) {
				const auto pos = part.find_last_not_of(PathDelimiters);
				if (pos != std::string::npos) {
					if (!result.empty())
						result.append(&PathDelimiter, 1);
					result.append(part.c_str(), pos + 1);
				}
			}
		}
		return result;
	}

	std::string PathUtil::Combine(const std::string & a, const std::string & b) {
		return Combine({a, b});
	}

	std::string PathUtil::GetDirectoryName(const std::string & path) {
		const auto pos = path.find_last_of(PathDelimiters);
		if (pos == std::string::npos)
			return "";

		return path.substr(0, pos);
	}

	std::string PathUtil::GetFilename(const std::string & path) {
		const auto pos = path.find_last_of(PathDelimiters);
		if (pos == std::string::npos)
			return path;

		return path.substr(pos + 1, path.size() - pos);
	}

	std::string PathUtil::GetFilenameWithoutExt(const std::string & path) {
		std::string filename = GetFilename(path);
		const auto pos = filename.find_last_of('.');
		if (pos == std::string::npos)
			return filename;

		return filename.substr(0, pos);
	}

	std::string PathUtil::GetExt(const std::string & path) {
		const auto pos = path.find_last_of('.');
		if (pos == std::string::npos)
			return "";

		std::string ext = path.substr(pos + 1, path.size() - pos);
		for (auto &ch: ext)
			ch = static_cast<char>(tolower(ch));
		return ext;
	}

	std::string PathUtil::Normalize(const std::string & path)
	{
		std::string result = path;
		std::ranges::replace(result, '/', '\\');
		return result;
	}

}
