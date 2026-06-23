#pragma once

#include <cstdint>
#include <expected>
#include <istream>
#include <optional>
#include <string>

namespace Chrivent {
	enum class ParseErrorCode {
		FileOpen,
		UnexpectedEnd,
		InvalidHeader,
		UnsupportedVersion,
		InvalidValue,
		InvalidCount,
		InvalidIndex
	};

	struct ParseError {
		ParseErrorCode code = ParseErrorCode::InvalidValue;
		std::string section;
		std::string message;
		std::streamoff offset = 0;
	};

	class BinaryReader {
		std::istream& stream;
		std::streampos end;
		const char* section = "file";
		std::optional<ParseError> error;

		// 현재 스트림 위치를 파일 시작 기준 바이트 오프셋으로 반환한다.
		std::streamoff ResolveOffset() const;

	public:
		explicit BinaryReader(std::istream& source);

		// 이후 오류에 기록할 파서 구간 이름을 지정한다.
		void SetSection(const char* value) { section = value; }
		// 아직 읽지 않은 바이트가 남아 있는지 확인한다.
		bool HasMore() const;
		// 현재 위치부터 파일 끝까지 남은 바이트 수를 반환한다.
		std::size_t RemainingBytes() const;
		// 첫 번째 파싱 오류를 기록한다.
		bool Fail(ParseErrorCode code, const std::string& message);
		// 지정한 바이트 수만큼 읽고 부족하면 오류를 기록한다.
		bool Read(void* destination, std::size_t bytes);
		// PMX 인덱스 크기 규칙에 따라 가변 크기 인덱스를 읽는다.
		bool ReadIndex(int32_t& index, uint8_t indexSize);
		// 부호 있는 개수를 읽고 범위와 최소 필요 바이트를 검증한다.
		bool ReadCount(int32_t& count, std::size_t minimumItemBytes = 1, std::size_t maximumCount = 10'000'000);
		// 부호 없는 개수를 읽고 범위와 최소 필요 바이트를 검증한다.
		bool ReadCount(uint32_t& count, std::size_t minimumItemBytes = 1, std::size_t maximumCount = 10'000'000);
		// 현재까지 기록된 오류를 expected 결과로 반환한다.
		std::expected<void, ParseError> Result() const;

		// 지정한 타입 크기만큼 바이너리 스트림에서 읽는다.
		template <typename T>
		bool Read(T& destination) {
			return Read(&destination, sizeof(T));
		}
	};

	// 파싱 오류를 파일 로그에 사용할 문자열로 변환한다.
	std::string FormatParseError(const ParseError& error);
}
