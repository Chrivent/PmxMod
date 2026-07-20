#include "Core/Text/TextEncoding.h"

#include <gtest/gtest.h>

namespace Chrivent {
	TEST(TextEncodingContract, ConvertsUtf8AndWideTextInBothDirections) {
		const std::u8string encodedText = u8"문자열 변환";
		std::string utf8;
		utf8.reserve(encodedText.size());
		for (const char8_t character : encodedText)
			utf8.push_back(static_cast<char>(character));
		const std::wstring wide = TextEncoding::Utf8ToWide(utf8);
		ASSERT_FALSE(wide.empty());
		EXPECT_EQ(TextEncoding::WideToUtf8(wide), utf8);
	}

	TEST(TextEncodingContract, ConvertsFixedLengthShiftJisWithoutNullTerminator) {
		constexpr char fixedName[] = {'A', 'B', 'C'};
		EXPECT_EQ(TextEncoding::ShiftJisToUtf8(fixedName, sizeof(fixedName)), "ABC");
	}

	TEST(TextEncodingContract, CreatesAFileSystemPathFromUtf8) {
		const std::u8string encodedPath = u8"폴더/파일.txt";
		std::string utf8Path;
		utf8Path.reserve(encodedPath.size());
		for (const char8_t character : encodedPath)
			utf8Path.push_back(static_cast<char>(character));
		EXPECT_EQ(TextEncoding::Utf8ToPath(utf8Path), std::filesystem::path(encodedPath));
	}
}
