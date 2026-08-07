#include "Core/Text/TextEncoding.h"

#include <gtest/gtest.h>

namespace Chrivent {
	TEST(TextEncodingContract, ConvertsUtf8AndWideTextInBothDirections) {
		const std::u8string encodedText = u8"문자열 변환";
		std::string utf8;
		utf8.reserve(encodedText.size());
		for (const char8_t character : encodedText)
			utf8.push_back(static_cast<char>(character));
		const auto wide = TextEncoding::TryUtf8ToWide(utf8);
		ASSERT_TRUE(wide);
		ASSERT_FALSE(wide->empty());
		const auto convertedUtf8 = TextEncoding::TryWideToUtf8(*wide);
		ASSERT_TRUE(convertedUtf8);
		EXPECT_EQ(*convertedUtf8, utf8);
	}

	TEST(TextEncodingContract, ConvertsFixedLengthShiftJisWithoutNullTerminator) {
		constexpr char fixedName[] = {'A', 'B', 'C'};
		const auto converted = TextEncoding::TryShiftJisToUtf8(fixedName, sizeof(fixedName));
		ASSERT_TRUE(converted);
		EXPECT_EQ(*converted, "ABC");
	}

	TEST(TextEncodingContract, CreatesAFileSystemPathFromUtf8) {
		const std::u8string encodedPath = u8"폴더/파일.txt";
		std::string utf8Path;
		utf8Path.reserve(encodedPath.size());
		for (const char8_t character : encodedPath)
			utf8Path.push_back(static_cast<char>(character));
		const auto path = TextEncoding::TryUtf8ToPath(utf8Path);
		ASSERT_TRUE(path);
		EXPECT_EQ(*path, std::filesystem::path(encodedPath));
	}

	TEST(TextEncodingContract, DistinguishesEmptyTextFromInvalidUtf8) {
		const auto emptyText = TextEncoding::TryUtf8ToWide({});
		ASSERT_TRUE(emptyText);
		EXPECT_TRUE(emptyText->empty());
		constexpr std::string invalidUtf8{"\xC3\x28", 2};
		const auto invalidText = TextEncoding::TryUtf8ToWide(invalidUtf8);
		ASSERT_FALSE(invalidText);
		EXPECT_EQ(invalidText.error(), TextEncoding::Error::InvalidSequence);
		const auto invalidPath = TextEncoding::TryUtf8ToPath(invalidUtf8);
		ASSERT_FALSE(invalidPath);
		EXPECT_EQ(invalidPath.error(), TextEncoding::Error::InvalidSequence);
	}
}
