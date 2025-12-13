#pragma once

#include <cstdio>
#include <cstdint>
#include <string>

#include "UnicodeUtil.h"

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
