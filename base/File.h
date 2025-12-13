#pragma once

#include <cstdio>
#include <cstdint>
#include <string>

enum class SeekDir
{
	Begin,
	End,
};

class File
{
public:
	File();
	~File();

	void Close();
	int64_t Tell() const;

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

	bool OpenFile(const char* filepath, const char* mode);

	FILE*	m_fp;
	int64_t	m_fileSize;
	bool	m_badFlag;

	std::string ReadAll() const;
};
