#include "BinaryReader.h"

namespace Chrivent {
	std::streampos BinaryReader::GetFileEnd(std::istream& is) {
		const auto origin = is.tellg();
		is.seekg(0, std::ios::end);
		const auto end = is.tellg();
		is.seekg(origin, std::ios::beg);
		return end;
	}

	bool BinaryReader::HasMore(std::istream& is, const std::streampos& end) {
		const auto cur = is.tellg();
		return cur != std::streampos(-1) && cur < end;
	}

	bool BinaryReader::Read(std::istream& is, void* dst, const std::size_t bytes) {
		return static_cast<bool>(is.read(static_cast<char*>(dst), static_cast<std::streamsize>(bytes)));
	}

	bool BinaryReader::ReadIndex(std::istream& is, int32_t* index, const uint8_t indexSize) {
		switch (indexSize) {
			case 1: {
				uint8_t idx;
				if (!Read(is, &idx))
					return false;
				if (idx != 0xFF)
					*index = static_cast<int32_t>(idx);
				else
					*index = -1;
			}
				return true;
			case 2: {
				uint16_t idx;
				if (!Read(is, &idx))
					return false;
				if (idx != 0xFFFF)
					*index = static_cast<int32_t>(idx);
				else
					*index = -1;
			}
				return true;
			case 4: {
				uint32_t idx;
				if (!Read(is, &idx))
					return false;
				*index = static_cast<int32_t>(idx);
			}
				return true;
			default:
				is.setstate(std::ios::failbit);
				return false;
		}
	}
}
