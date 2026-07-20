#include "Core/Parser/BinaryReader.h"

#include <gtest/gtest.h>
#include <sstream>

namespace Chrivent {
	TEST(BinaryReaderContract, ReportsUnexpectedEndWithoutChangingTheFirstError) {
		std::istringstream stream(std::string("\x01\x02", 2));
		BinaryReader reader(stream);
		uint32_t value = 0;
		EXPECT_FALSE(reader.Read(value));
		EXPECT_FALSE(reader.Fail(ParseErrorCode::InvalidValue, "두 번째 오류"));
		const auto result = reader.Result();
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::UnexpectedEnd);
		EXPECT_EQ(result.error().offset, 0);
	}

	TEST(BinaryReaderContract, RejectsCountsThatExceedTheRemainingBytes) {
		std::string bytes(sizeof(uint32_t), '\0');
		bytes[0] = 2;
		std::istringstream stream(bytes);
		BinaryReader reader(stream);
		uint32_t count = 0;
		EXPECT_FALSE(reader.ReadCount(count, sizeof(uint32_t)));
		const auto result = reader.Result();
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::InvalidCount);
	}

	TEST(BinaryReaderContract, ReadsVariableWidthMissingIndices) {
		std::istringstream stream(std::string("\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 7));
		BinaryReader reader(stream);
		int32_t index = 0;
		EXPECT_TRUE(reader.ReadIndex(index, 1));
		EXPECT_EQ(index, -1);
		EXPECT_TRUE(reader.ReadIndex(index, 2));
		EXPECT_EQ(index, -1);
		EXPECT_TRUE(reader.ReadIndex(index, 4));
		EXPECT_EQ(index, -1);
		EXPECT_TRUE(reader.Result());
	}
}
