#include "Core/Parser/BinaryReader.h"

#include <sstream>

namespace Chrivent {
	BinaryReader::BinaryReader(std::istream& source) : stream(source), end(source.tellg()) {
		stream.seekg(0, std::ios::end);
		end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		if (end == std::streampos(-1))
			Fail(ParseErrorCode::UnexpectedEnd, "파일 크기를 확인할 수 없습니다.");
	}

	std::streamoff BinaryReader::ResolveOffset() const {
		const auto position = stream.tellg();
		return position == std::streampos(-1) ? 0 : position - std::streampos(0);
	}

	bool BinaryReader::HasMore() const {
		if (error)
			return false;
		const auto position = stream.tellg();
		return position != std::streampos(-1) && position < end;
	}

	std::size_t BinaryReader::RemainingBytes() const {
		if (error)
			return 0;
		const auto position = stream.tellg();
		if (position == std::streampos(-1) || position >= end)
			return 0;
		const auto remaining = end - position;
		return static_cast<std::size_t>(remaining);
	}

	bool BinaryReader::Fail(const ParseErrorCode code, const std::string& message) {
		if (!error)
			error = ParseError{ code, section, message, ResolveOffset() };
		return false;
	}

	bool BinaryReader::Read(void* destination, const std::size_t bytes) {
		if (error)
			return false;
		if (bytes > RemainingBytes())
			return Fail(ParseErrorCode::UnexpectedEnd, "필요한 데이터보다 파일이 먼저 끝났습니다.");
		if (bytes == 0)
			return true;
		stream.read(static_cast<char*>(destination), static_cast<std::streamsize>(bytes));
		if (!stream)
			return Fail(ParseErrorCode::UnexpectedEnd, "바이너리 데이터를 읽지 못했습니다.");
		return true;
	}

	bool BinaryReader::ReadIndex(int32_t& index, const uint8_t indexSize) {
		switch (indexSize) {
			case 1: {
				uint8_t value = 0;
				if (!Read(value))
					return false;
				index = value == 0xFF ? -1 : value;
				return true;
			}
			case 2: {
				uint16_t value = 0;
				if (!Read(value))
					return false;
				index = value == 0xFFFF ? -1 : value;
				return true;
			}
			case 4: {
				uint32_t value = 0;
				if (!Read(value))
					return false;
				if (value > static_cast<uint32_t>(INT32_MAX) && value != UINT32_MAX)
					return Fail(ParseErrorCode::InvalidIndex, "32비트 인덱스가 지원 범위를 초과했습니다.");
				index = value == UINT32_MAX ? -1 : static_cast<int32_t>(value);
				return true;
			}
			default:
				return Fail(ParseErrorCode::InvalidHeader, "인덱스 크기는 1, 2, 4바이트 중 하나여야 합니다.");
		}
	}

	bool BinaryReader::ReadCount(int32_t& count, const std::size_t minimumItemBytes, const std::size_t maximumCount) {
		if (!Read(count))
			return false;
		if (count < 0)
			return Fail(ParseErrorCode::InvalidCount, "항목 개수가 음수입니다.");
		const auto unsignedCount = static_cast<uint32_t>(count);
		if (unsignedCount > maximumCount)
			return Fail(ParseErrorCode::InvalidCount, "항목 개수가 허용 범위를 초과했습니다.");
		if (minimumItemBytes != 0 && unsignedCount > RemainingBytes() / minimumItemBytes)
			return Fail(ParseErrorCode::InvalidCount, "항목 개수에 필요한 데이터가 파일에 없습니다.");
		return true;
	}

	bool BinaryReader::ReadCount(uint32_t& count, const std::size_t minimumItemBytes, const std::size_t maximumCount) {
		if (!Read(count))
			return false;
		if (count > maximumCount)
			return Fail(ParseErrorCode::InvalidCount, "항목 개수가 허용 범위를 초과했습니다.");
		if (minimumItemBytes != 0 && count > RemainingBytes() / minimumItemBytes)
			return Fail(ParseErrorCode::InvalidCount, "항목 개수에 필요한 데이터가 파일에 없습니다.");
		return true;
	}

	std::expected<void, ParseError> BinaryReader::Result() const {
		if (error)
			return std::unexpected(*error);
		return {};
	}

	std::string BinaryReader::FormatParseError(const ParseError& error) {
		std::ostringstream message;
		message << error.section << " (오프셋 " << error.offset << "): " << error.message;
		return message.str();
	}
}
