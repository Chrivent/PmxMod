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

	void BinaryReader::ReadIndex(std::istream& is, int32_t* index, const uint8_t indexSize) {
		switch (indexSize) {
			case 1: {
				uint8_t idx;
				Read(is, &idx);
				if (idx != 0xFF)
					*index = idx;
				else
					*index = -1;
				break;
			}
			case 2: {
				uint16_t idx;
				Read(is, &idx);
				if (idx != 0xFFFF)
					*index = idx;
				else
					*index = -1;
				break;
			}
			case 4: {
				uint32_t idx;
				Read(is, &idx);
				*index = idx;
				break;
			}
			default:
				break;
		}
	}
}
