#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <windows.h>

struct UnicodeUtil {
    static std::string ToUtf8String(const std::wstring& w) {
        if (w.empty())
            return {};
        const int need = WideCharToMultiByte(
            CP_UTF8, 0,
            w.c_str(), -1,
            nullptr, 0,
            nullptr, nullptr);
        if (need <= 0)
            return {};
        std::string out(static_cast<size_t>(need), '\0');
        const int written = WideCharToMultiByte(
            CP_UTF8, 0,
            w.c_str(), -1,
            out.data(), need,
            nullptr, nullptr);
        if (written <= 0)
            return {};
        if (!out.empty() && out.back() == '\0')
            out.pop_back();
        return out;
    }

    static bool TryToWString(const std::string& utf8, std::wstring& out) {
        out.clear();
        if (utf8.empty())
            return true;
        const int need = MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), static_cast<int>(utf8.size()),
            nullptr, 0);
        if (need <= 0)
            return false;
        out.resize(static_cast<size_t>(need));
        return MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8.data(), static_cast<int>(utf8.size()),
            out.data(), need) == need;
    }

    static bool ConvU16ToU8(const std::u16string& u16, std::string& out) {
        out.clear();
        if (u16.empty())
            return true;
        const auto w = reinterpret_cast<const wchar_t*>(u16.data());
        const int w_len = static_cast<int>(u16.size());
        const int need = WideCharToMultiByte(
            CP_UTF8, 0,
            w, w_len,
            nullptr, 0,
            nullptr, nullptr);
        if (need <= 0)
            return false;
        out.resize(static_cast<size_t>(need));
        return WideCharToMultiByte(
            CP_UTF8, 0,
            w, w_len,
            out.data(), need,
            nullptr, nullptr) == need;
    }

    static std::u16string ConvertSjisToU16String(const char* sjis) {
        if (!sjis)
            return {};
        const int need = MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, -1,
            nullptr, 0);
        if (need <= 0)
            return {};
        std::wstring w;
        w.resize(static_cast<size_t>(need));
        if (MultiByteToWideChar(
            932, MB_ERR_INVALID_CHARS,
            sjis, -1,
            w.data(), need) <= 0)
            return {};
        if (!w.empty() && w.back() == L'\0')
            w.pop_back();
        return std::u16string(reinterpret_cast<const char16_t*>(w.data()), w.size());
    }
};

struct File {
	~File() {
		Close();
	}

	bool OpenFile(const char* filepath, const char* mode) {
		Close();
		std::wstring wFilepath, wMode;
		if (!UnicodeUtil::TryToWString(filepath, wFilepath))
			return false;
		if (!UnicodeUtil::TryToWString(mode, wMode))
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
