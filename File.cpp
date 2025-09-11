#include "File.h"
#include "UnicodeUtil.h"

#include <iterator>

namespace saba
{
	File::File()
		: m_fp(nullptr)
		, m_fileSize(0)
		, m_badFlag(false) {
	}

	File::~File() {
		Close();
	}

	bool File::OpenFile(const char * filepath, const char * mode) {
		if (m_fp != nullptr)
			Close();
		std::wstring wFilepath;
		if (!TryToWString(filepath, wFilepath))
			return false;
		std::wstring wMode;
		if (!TryToWString(mode, wMode))
			return false;
		const auto err = _wfopen_s(&m_fp, wFilepath.c_str(), wMode.c_str());
		if (err != 0)
			return false;

		m_badFlag = false;

		Seek(0, SeekDir::End);
		m_fileSize = Tell();
		Seek(0, SeekDir::Begin);
		if (m_badFlag) {
			Close();
			return false;
		}
		return true;
	}

	void File::Close() {
		if (m_fp != nullptr) {
			fclose(m_fp);
			m_fp = nullptr;
			m_fileSize = 0;
			m_badFlag = false;
		}
	}

	bool File::Seek(const int64_t offset, const SeekDir origin) {
		if (m_fp == nullptr)
			return false;
		int cOrigin = 0;
		switch (origin) {
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
		if (_fseeki64(m_fp, offset, cOrigin) != 0) {
			m_badFlag = true;
			return false;
		}
		return true;
	}

	int64_t File::Tell() const {
		if (m_fp == nullptr)
			return -1;
		return _ftelli64(m_fp);
	}

	std::string File::ReadAll() const {
		std::string all;

		if (m_fp != nullptr) {
			int ch = fgetc(m_fp);
			while (ch != EOF) {
				all.push_back(ch);
				ch = fgetc(m_fp);
			}
		}
		return all;
	}
}

