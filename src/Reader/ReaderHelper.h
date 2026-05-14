#pragma once

#include <filesystem>

namespace ReaderHelper {
	// 현재 위치를 보존한 채 스트림의 끝 위치를 구한다.
	std::streampos GetFileEnd(std::istream& is);
	// 저장해 둔 끝 위치 이전에 읽을 데이터가 남아 있는지 확인한다.
	bool HasMore(std::istream& is, const std::streampos& end);
	// 지정한 바이트 수만큼 바이너리 스트림에서 읽는다.
	void Read(std::istream& is, void* dst, std::size_t bytes);
	// PMX 헤더의 인덱스 크기 규칙에 맞춰 가변 크기 인덱스를 읽는다.
	void ReadIndex(std::istream& is, int32_t* index, uint8_t indexSize);
	// POD 값을 바이너리 스트림에서 읽는다.
	template <typename T>
	static void Read(std::istream& is, T* dst) {
		Read(is, dst, sizeof(T));
	}
}
