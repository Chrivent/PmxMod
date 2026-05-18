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

	void BinaryReader::Read(std::istream& is, void* dst, const std::size_t bytes) {
		if (!is.read(static_cast<char*>(dst), static_cast<std::streamsize>(bytes)))
			throw std::runtime_error("Failed to read binary stream.");
	}

	void BinaryReader::ReadIndex(std::istream& is, int32_t* index, const uint8_t indexSize) {
		switch (indexSize) {
			case 1: {
				uint8_t idx;
				Read(is, &idx);
				if (idx != 0xFF)
					*index = static_cast<int32_t>(idx);
				else
					*index = -1;
			}
				break;
			case 2: {
				uint16_t idx;
				Read(is, &idx);
				if (idx != 0xFFFF)
					*index = static_cast<int32_t>(idx);
				else
					*index = -1;
			}
				break;
			case 4: {
				uint32_t idx;
				Read(is, &idx);
				*index = static_cast<int32_t>(idx);
			}
				break;
			default:
				throw std::runtime_error("Invalid PMX index size.");
		}
	}
}
