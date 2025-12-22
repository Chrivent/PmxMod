#pragma once

#include <cstdio>
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

struct File {
	~File() {
		Close();
	}

	bool OpenFile(const std::filesystem::path& filepath, const std::wstring& mode) {
		Close();
		const std::wstring wFilepath = filepath.wstring();
		if (wFilepath.empty())
			return false;
		if (mode.empty())
			return false;
		if (_wfopen_s(&m_fp, wFilepath.c_str(), mode.c_str()) != 0 || !m_fp)
			return false;
		m_badFlag = false;
		auto Seek = [&](const int64_t offset, const int origin) {
			if (m_fp == nullptr)
				return false;
			if (_fseeki64(m_fp, offset, origin) != 0) {
				m_badFlag = true;
				return false;
			}
			return true;
		};
		if (!Seek(0, 2)) {
			Close();
			return false;
		}
		m_fileSize = Tell();
		if (!Seek(0, 0)) {
			Close();
			return false;
		}
		return true;
	}

	void Close() {
		if (m_fp != nullptr) {
			fclose(m_fp);
			m_fp = nullptr;
			m_fileSize = 0;
			m_badFlag = false;
		}
	}

	int64_t Tell() const {
		return m_fp ? _ftelli64(m_fp) : -1;
	}

	std::string ReadAll() const {
		std::string all;
		if (!m_fp)
			return all;
		for (int ch = fgetc(m_fp); ch != -1; ch = fgetc(m_fp))
			all.push_back(static_cast<char>(ch));
		return all;
	}

	template <typename T>
	bool Read(T* buffer, size_t count = 1) {
		if (buffer == nullptr)
			return false;
		if (m_fp == nullptr)
			return false;
		if (fread_s(buffer, sizeof(T) * count, sizeof(T), count, m_fp) != count) {
			m_badFlag = true;
			return false;
		}
		return true;
	}

	FILE*	m_fp = nullptr;
	int64_t	m_fileSize = 0;
	bool	m_badFlag = false;
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
