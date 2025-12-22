#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <windows.h>

struct UnicodeUtil {
    static std::string WStringToUtf8(const std::wstring& w) {
        if (w.empty())
            return {};
        const int need = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), static_cast<int>(w.size()),
            nullptr, 0, nullptr, nullptr);
        if (need <= 0)
        	return {};
        std::string utf8(static_cast<size_t>(need), '\0');
        const int written = WideCharToMultiByte(
            CP_UTF8, 0,
            w.data(), static_cast<int>(w.size()),
            utf8.data(), need, nullptr, nullptr);
        if (written != need)
        	return {};
        return utf8;
    }

    static std::string SjisToUtf8(const char* sjis) {
    	if (!sjis)
    		return {};
    	const int need = MultiByteToWideChar(
			932, MB_ERR_INVALID_CHARS,
			sjis, -1,
			nullptr, 0);
    	if (need <= 0)
    		return {};
    	std::wstring w(static_cast<size_t>(need), L'\0');
    	const int written = MultiByteToWideChar(
			932, MB_ERR_INVALID_CHARS,
			sjis, -1,
			w.data(), need);
    	if (written <= 0)
    		return {};
    	if (!w.empty() && w.back() == L'\0')
    		w.pop_back();
    	return WStringToUtf8(w);
    }
};

struct PathUtil {
    static std::filesystem::path GetExecutablePath() {
    	std::vector<wchar_t> buf(MAX_PATH);
    	while (true) {
    		const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    		if (n == 0)
    			return {};
    		if (n < buf.size() - 1)
    			return std::filesystem::path(buf.data(), buf.data() + n);
    		buf.resize(buf.size() * 2);
    	}
    }

    static std::string GetExt(const std::filesystem::path& path) {
    	std::string ext = path.extension().string();
    	if (!ext.empty() && ext[0] == '.')
    		ext.erase(ext.begin());
    	for (auto& ch : ext)
    		ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    	return ext;
    }
};
