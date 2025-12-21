#pragma once

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
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
private:
    static std::string U8ToString(const std::filesystem::path& p) {
        const std::u8string u8 = p.u8string();
        return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
    }

    static std::filesystem::path StringToU8(const std::string& s) {
        std::u8string u8;
        u8.resize(s.size());
        for (size_t i = 0; i < s.size(); i++)
            u8[i] = static_cast<char8_t>(static_cast<unsigned char>(s[i]));
        return std::filesystem::path(u8);
    }

public:
    static std::string GetExecutablePath() {
        auto ExecutablePath = []() -> std::filesystem::path {
            std::vector<wchar_t> buf(260);
            for (;;) {
                const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
                if (n == 0) return {};
                if (n < buf.size() - 1) return std::filesystem::path(std::wstring(buf.data(), n));
                buf.resize(buf.size() * 2);
            }
        };
        std::filesystem::path p = ExecutablePath();
        p.make_preferred();
        return U8ToString(p);
    }

    static std::string Combine(const std::string& a, const std::string& b) {
        const std::filesystem::path pa = StringToU8(a);
        const std::filesystem::path pb = StringToU8(b);
        std::filesystem::path out = pa / pb;
        out = out.lexically_normal();
        out.make_preferred();
        return U8ToString(out);
    }

    static std::string GetDirectoryName(const std::string& path) {
        std::filesystem::path p = StringToU8(path);
        p.make_preferred();
        return U8ToString(p.parent_path());
    }

    static std::string GetExt(const std::string& path) {
        const std::filesystem::path p = StringToU8(path);
        auto ext = p.extension().wstring();
        if (ext.empty()) return {};
        if (!ext.empty() && ext[0] == L'.')
            ext.erase(ext.begin());
        std::string out = U8ToString(std::filesystem::path(ext));
        for (char& ch : out)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return out;
    }

    static std::string Normalize(const std::string& path) {
        std::filesystem::path p = StringToU8(path);
        p = p.lexically_normal();
        p.make_preferred();
        return U8ToString(p);
    }
};
