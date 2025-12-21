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

	static std::wstring Utf8ToWString(const std::string& utf8) {
    	if (utf8.empty())
    		return {};
    	const int need = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS,
			utf8.data(), static_cast<int>(utf8.size()),
			nullptr, 0);
    	if (need <= 0)
    		return {};
    	std::wstring w;
    	w.resize(static_cast<size_t>(need), L'\0');
    	const int written = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS,
			utf8.data(), static_cast<int>(utf8.size()),
			w.data(), need);
    	if (written != need)
    		return {};
    	return w;
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

	bool OpenFile(const char* filepath, const char* mode) {
		Close();
		const std::wstring wFilepath = UnicodeUtil::Utf8ToWString(filepath);
		const std::wstring wMode = UnicodeUtil::Utf8ToWString(mode);
		if (wFilepath.empty())
			return false;
		if (wMode.empty())
			return false;
		if (_wfopen_s(&m_fp, wFilepath.c_str(), wMode.c_str()) != 0 || !m_fp)
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
private:
	static std::string PathToUtf8(const std::filesystem::path& p) {
		const auto u8 = p.u8string();
		return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
	}

	static std::filesystem::path Utf8ToPath(const std::string& s) {
		return std::filesystem::path(
			std::u8string(reinterpret_cast<const char8_t*>(s.data()),
				reinterpret_cast<const char8_t*>(s.data() + s.size())));
	}

public:
    static std::string GetExecutablePath() {
    	std::vector<wchar_t> buf(260);
    	while (true) {
    		const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    		if (n == 0)
    			return {};
    		if (n < buf.size() - 1) {
    			std::filesystem::path p(std::wstring(buf.data(), n));
    			p.make_preferred();
    			return PathToUtf8(p);
    		}
    		buf.resize(buf.size() * 2);
    	}
    }

    static std::string Combine(const std::string& a, const std::string& b) {
    	auto out = (Utf8ToPath(a) / Utf8ToPath(b)).lexically_normal();
    	out.make_preferred();
    	return PathToUtf8(out);
    }

    static std::string GetDirectoryName(const std::string& path) {
    	auto p = Utf8ToPath(path);
    	p.make_preferred();
    	return PathToUtf8(p.parent_path());
    }

    static std::string GetExt(const std::string& path) {
    	const auto ext = Utf8ToPath(path).extension().u8string();
    	if (ext.empty())
    		return {};
    	const size_t start = ext[0] == u8'.' ? 1 : 0;
    	std::string out(reinterpret_cast<const char*>(ext.data() + start), ext.size() - start);
    	for (char& ch : out)
    		ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    	return out;
    }

    static std::string Normalize(const std::string& path) {
    	auto out = Utf8ToPath(path).lexically_normal();
    	out.make_preferred();
    	return PathToUtf8(out);
    }
};
