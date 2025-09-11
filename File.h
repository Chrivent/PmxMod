#pragma once

#include <cstdio>
#include <vector>
#include <cstdint>
#include <string>

namespace saba
{
	class File
	{
	public:
		File();
		~File();

		File(const File&) = delete;
		File& operator = (const File&) = delete;

		void Close();
		bool IsOpen();
		void ClearBadFlag();
		bool IsEOF();

		FILE* GetFilePointer() const;

		bool ReadAll(std::vector<char>* buffer);
		bool ReadAll(std::vector<uint8_t>* buffer);
		bool ReadAll(std::vector<int8_t>* buffer);

		enum class SeekDir
		{
			Begin,
			Current,
			End,
		};
		bool Seek(int64_t offset, SeekDir origin);
		int64_t Tell();

		template <typename T>
		bool Read(T* buffer, size_t count = 1)
		{
			if (buffer == nullptr)
			{
				return false;
			}

			if (!IsOpen())
			{
				return false;
			}
			if (fread_s(buffer, sizeof(T) * count, sizeof(T), count, m_fp) != count)
			{
				m_badFlag = true;
				return false;
			}
			return true;
		}

		template <typename T>
		bool Write(T* buffer, size_t count = 1)
		{
			if (buffer == nullptr)
			{
				return false;
			}

			if (!IsOpen())
			{
				return false;
			}

			if (fwrite(buffer, sizeof(T), count, m_fp) != count)
			{
				m_badFlag = true;
				return false;
			}
			return true;
		}

		bool OpenFile(const char* filepath, const char* mode);

		FILE*	m_fp;
		int64_t	m_fileSize;
		bool	m_badFlag;
	};

	class TextFileReader
	{
	public:
		TextFileReader() = default;
		explicit TextFileReader(const char* filepath);
		explicit TextFileReader(const std::string& filepath);

		bool Open(const char* filepath);
		bool Open(const std::string& filepath);
		void Close();
		bool IsOpen();

		std::string ReadLine();
		void ReadAllLines(std::vector<std::string>& lines);
		std::string ReadAll();
		bool IsEof();

		File	m_file;
	};
}
