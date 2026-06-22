#pragma once

#include <cstdint>
#include <istream>

namespace Chrivent {
	class BinaryReader {
	public:
		static std::streampos GetFileEnd(std::istream& is);
		// 저장해 둔 끝 위치 이전에 읽을 데이터가 남아 있는지 확인한다.
		static bool HasMore(std::istream& is, const std::streampos& end);
		// 지정한 바이트 수만큼 바이너리 스트림에서 읽는다.
		static void Read(std::istream& is, void* dst, const std::size_t bytes) {
			is.read(static_cast<char*>(dst), bytes);
		}
		// PMX 헤더의 인덱스 크기 규칙에 맞춰 가변 크기 인덱스를 읽는다.
		static void ReadIndex(std::istream& is, int32_t* index, uint8_t indexSize);
		
		// 지정한 타입 크기만큼 바이너리 스트림에서 읽는다.
		template <typename T>
		static void Read(std::istream& is, T* dst) {
			Read(is, dst, sizeof(T));
		}
	};
}
