#include "File.h"
#include "UnicodeUtil.h"

#include <iterator>

namespace saba
{
	File::File()
		: m_fp(nullptr)
		, m_fileSize(0)
		, m_badFlag(false)
	{
	}

	File::~File()
	{
		Close();
	}

	bool File::OpenFile(const char * filepath, const char * mode)
	{
		if (m_fp != nullptr)
		{
			Close();
		}
		std::wstring wFilepath;
		if (!TryToWString(filepath, wFilepath))
		{
			return false;
		}
		std::wstring wMode;
		if (!TryToWString(mode, wMode))
		{
			return false;
		}
		auto err = _wfopen_s(&m_fp, wFilepath.c_str(), wMode.c_str());
		if (err != 0)
		{
			return false;
		}

		ClearBadFlag();

		Seek(0, SeekDir::End);
		m_fileSize = Tell();
		Seek(0, SeekDir::Begin);
		if (m_badFlag)
		{
			Close();
			return false;
		}
		return true;
	}

	void File::Close()
	{
		if (m_fp != nullptr)
		{
			fclose(m_fp);
			m_fp = nullptr;
			m_fileSize = 0;
			m_badFlag = false;
		}
	}

	bool File::IsOpen()
	{
		return m_fp != nullptr;
	}

	void File::ClearBadFlag()
	{
		m_badFlag = false;
	}

	bool File::IsEOF()
	{
		return feof(m_fp) != 0;
	}

	bool File::ReadAll(std::vector<char>* buffer)
	{
		if (buffer == nullptr)
		{
			return false;
		}

		buffer->resize(m_fileSize);
		Seek(0, SeekDir::Begin);
		if (!Read((*buffer).data(), m_fileSize))
		{
			return false;
		}

		return true;
	}

	bool File::ReadAll(std::vector<uint8_t>* buffer)
	{
		if (buffer == nullptr)
		{
			return false;
		}

		buffer->resize(m_fileSize);
		Seek(0, SeekDir::Begin);
		if (!Read((*buffer).data(), m_fileSize))
		{
			return false;
		}

		return true;
	}
	bool File::ReadAll(std::vector<int8_t>* buffer)
	{
		if (buffer == nullptr)
		{
			return false;
		}

		buffer->resize(m_fileSize);
		Seek(0, SeekDir::Begin);
		if (!Read((*buffer).data(), m_fileSize))
		{
			return false;
		}

		return true;
	}

	bool File::Seek(int64_t offset, SeekDir origin)
	{
		if (m_fp == nullptr)
		{
			return false;
		}
		int cOrigin = 0;
		switch (origin)
		{
		case SeekDir::Begin:
			cOrigin = SEEK_SET;
			break;
		case SeekDir::Current:
			cOrigin = SEEK_CUR;
			break;
		case SeekDir::End:
			cOrigin = SEEK_END;
			break;
		default:
			return false;
		}
		if (_fseeki64(m_fp, offset, cOrigin) != 0)
		{
			m_badFlag = true;
			return false;
		}
		return true;
	}

	int64_t File::Tell()
	{
		if (m_fp == nullptr)
		{
			return -1;
		}
		return _ftelli64(m_fp);
	}

	std::string TextFileReader::ReadLine()
	{
		if (!m_file.IsOpen() || IsEof())
		{
			return "";
		}

		std::string line;
		auto outputIt = std::back_inserter(line);
		int ch;
		ch = fgetc(m_file.m_fp);
		while (ch != EOF && ch != '\r' && ch != '\n')
		{
			line.push_back(ch);
			ch = fgetc(m_file.m_fp);
		}
		if (ch != EOF)
		{
			if (ch == '\r')
			{
				ch = fgetc(m_file.m_fp);
				if (ch != EOF && ch != '\n')
				{
					ungetc(ch, m_file.m_fp);
				}
			}
			else
			{
				ch = fgetc(m_file.m_fp);
				if (ch != EOF)
				{
					ungetc(ch, m_file.m_fp);
				}
			}
		}

		return line;
	}

	void TextFileReader::ReadAllLines(std::vector<std::string>& lines)
	{
		lines.clear();
		if (!m_file.IsOpen() || IsEof())
		{
			return;
		}
		while (!IsEof())
		{
			lines.emplace_back(ReadLine());
		}
	}

	std::string TextFileReader::ReadAll()
	{
		std::string all;

		if (m_file.IsOpen())
		{
			int ch = fgetc(m_file.m_fp);
			while (ch != EOF)
			{
				all.push_back(ch);
				ch = fgetc(m_file.m_fp);
			}
		}
		return all;
	}

	bool TextFileReader::IsEof()
	{
		if (!m_file.IsOpen())
		{
			return false;
		}
		return m_file.IsEOF();
	}
}

