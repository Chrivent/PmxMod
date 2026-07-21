#include "Core/Parser/PmxParser.h"
#include "Core/Parser/VmdParser.h"

#include <gtest/gtest.h>
#include <limits>
#include <sstream>
#include <string>

namespace Chrivent {
	// 파서 계약 테스트에 필요한 최소 PMX/VMD 바이너리를 생성한다.
	class ParserContractTest : public testing::Test {
	protected:
		template <typename T>
		static void Append(std::string& bytes, const T& value) {
			const auto* data = reinterpret_cast<const char*>(&value);
			bytes.append(data, sizeof(T));
		}

		static void AppendBytes(std::string& bytes, const char* data, const std::size_t size) {
			bytes.append(data, size);
		}

		static void AppendEmptyPmxSections(std::string& bytes) {
			constexpr int32_t emptyCount = 0;
			for (int section = 0; section < 8; section++)
				Append(bytes, emptyCount);
		}

		static std::string BuildPmxWithMissingWeightedBone() {
			std::string bytes;
			AppendBytes(bytes, "PMX ", 4);
			Append(bytes, 2.0f);
			constexpr uint8_t headerData[] = {8, 1, 0, 1, 1, 1, 1, 1, 1};
			AppendBytes(bytes, reinterpret_cast<const char*>(headerData), sizeof(headerData));
			constexpr int32_t emptyStringLength = 0;
			for (int stringIndex = 0; stringIndex < 4; stringIndex++)
				Append(bytes, emptyStringLength);
			constexpr int32_t vertexCount = 1;
			Append(bytes, vertexCount);
			Append(bytes, glm::vec3(0));
			Append(bytes, glm::vec3(0, 1, 0));
			Append(bytes, glm::vec2(0));
			Append(bytes, PmxParser::WeightType::BoneDeform1);
			constexpr uint8_t missingBoneIndex = 0xFF;
			Append(bytes, missingBoneIndex);
			Append(bytes, 1.0f);
			AppendEmptyPmxSections(bytes);
			return bytes;
		}

		static std::string BuildPmxWithInvalidUtf8String() {
			std::string bytes;
			AppendBytes(bytes, "PMX ", 4);
			Append(bytes, 2.0f);
			constexpr uint8_t headerData[] = {8, 1, 0, 1, 1, 1, 1, 1, 1};
			AppendBytes(bytes, reinterpret_cast<const char*>(headerData), sizeof(headerData));
			constexpr int32_t invalidStringLength = 1;
			Append(bytes, invalidStringLength);
			constexpr char invalidUtf8 = static_cast<char>(0xFF);
			Append(bytes, invalidUtf8);
			constexpr int32_t emptyCount = 0;
			for (int stringIndex = 0; stringIndex < 3; stringIndex++)
				Append(bytes, emptyCount);
			for (int sectionIndex = 0; sectionIndex < 9; sectionIndex++)
				Append(bytes, emptyCount);
			return bytes;
		}

		static std::string BuildMinimalVmd() {
			std::string bytes(50, '\0');
			constexpr char signature[] = "Vocaloid Motion Data 0002";
			std::memcpy(bytes.data(), signature, sizeof(signature) - 1);
			constexpr uint32_t motionCount = 0;
			Append(bytes, motionCount);
			return bytes;
		}

		static std::string BuildPmxWithInvalidCommonToonIndex() {
			std::string bytes;
			AppendBytes(bytes, "PMX ", 4);
			Append(bytes, 2.0f);
			constexpr uint8_t headerData[] = {8, 1, 0, 1, 1, 1, 1, 1, 1};
			AppendBytes(bytes, reinterpret_cast<const char*>(headerData), sizeof(headerData));
			constexpr int32_t emptyStringLength = 0;
			for (int stringIndex = 0; stringIndex < 4; stringIndex++)
				Append(bytes, emptyStringLength);
			constexpr int32_t emptyCount = 0;
			Append(bytes, emptyCount);
			Append(bytes, emptyCount);
			Append(bytes, emptyCount);
			constexpr int32_t materialCount = 1;
			Append(bytes, materialCount);
			Append(bytes, emptyStringLength);
			Append(bytes, emptyStringLength);
			Append(bytes, glm::vec4(1));
			Append(bytes, glm::vec3(0));
			Append(bytes, 1.0f);
			Append(bytes, glm::vec3(0));
			Append(bytes, PmxParser::DrawModeFlags{});
			Append(bytes, glm::vec4(0));
			Append(bytes, 0.0f);
			constexpr uint8_t missingTextureIndex = 0xFF;
			Append(bytes, missingTextureIndex);
			Append(bytes, missingTextureIndex);
			Append(bytes, PmxParser::SphereMode::None);
			Append(bytes, PmxParser::ToonMode::Common);
			constexpr uint8_t invalidToonIndex = 10;
			Append(bytes, invalidToonIndex);
			Append(bytes, emptyStringLength);
			Append(bytes, emptyCount);
			for (int section = 0; section < 5; section++)
				Append(bytes, emptyCount);
			return bytes;
		}

		static std::string BuildPmxWithZeroLengthNormal() {
			std::string bytes;
			AppendBytes(bytes, "PMX ", 4);
			Append(bytes, 2.0f);
			constexpr uint8_t headerData[] = {8, 1, 0, 1, 1, 1, 1, 1, 1};
			AppendBytes(bytes, reinterpret_cast<const char*>(headerData), sizeof(headerData));
			constexpr int32_t emptyCount = 0;
			for (int stringIndex = 0; stringIndex < 4; stringIndex++)
				Append(bytes, emptyCount);
			constexpr int32_t vertexCount = 1;
			Append(bytes, vertexCount);
			Append(bytes, glm::vec3(0));
			Append(bytes, glm::vec3(0));
			Append(bytes, glm::vec2(0));
			Append(bytes, PmxParser::WeightType::BoneDeform1);
			constexpr uint8_t boneIndex = 0;
			Append(bytes, boneIndex);
			Append(bytes, 1.0f);
			Append(bytes, emptyCount);
			Append(bytes, emptyCount);
			Append(bytes, emptyCount);
			constexpr int32_t boneCount = 1;
			Append(bytes, boneCount);
			Append(bytes, emptyCount);
			Append(bytes, emptyCount);
			Append(bytes, glm::vec3(0));
			constexpr uint8_t missingBoneIndex = 0xFF;
			Append(bytes, missingBoneIndex);
			Append(bytes, emptyCount);
			Append(bytes, PmxParser::BoneFlags{});
			Append(bytes, glm::vec3(0));
			for (int section = 0; section < 4; section++)
				Append(bytes, emptyCount);
			return bytes;
		}

		static std::string BuildVmdWithInvalidShowFlag() {
			std::string bytes = BuildMinimalVmd();
			constexpr uint32_t emptyCount = 0;
			for (int section = 0; section < 4; section++)
				Append(bytes, emptyCount);
			constexpr uint32_t ikCount = 1;
			constexpr uint32_t frame = 0;
			constexpr uint8_t invalidShow = 2;
			Append(bytes, ikCount);
			Append(bytes, frame);
			Append(bytes, invalidShow);
			Append(bytes, emptyCount);
			return bytes;
		}

		static std::string BuildVmdWithNonFiniteMorphWeight() {
			std::string bytes = BuildMinimalVmd();
			constexpr uint32_t morphCount = 1;
			constexpr char morphName[15]{};
			constexpr uint32_t frame = 0;
			Append(bytes, morphCount);
			AppendBytes(bytes, morphName, sizeof(morphName));
			Append(bytes, frame);
			constexpr float invalidWeight = std::numeric_limits<float>::quiet_NaN();
			Append(bytes, invalidWeight);
			return bytes;
		}

		static std::string BuildVmdWithInvalidCameraFov() {
			std::string bytes = BuildMinimalVmd();
			constexpr uint32_t emptyCount = 0;
			Append(bytes, emptyCount);
			constexpr uint32_t cameraCount = 1;
			Append(bytes, cameraCount);
			constexpr uint32_t frame = 0;
			Append(bytes, frame);
			Append(bytes, 0.0f);
			Append(bytes, glm::vec3(0));
			Append(bytes, glm::vec3(0));
			constexpr uint8_t interpolation[24]{};
			AppendBytes(bytes, reinterpret_cast<const char*>(interpolation), sizeof(interpolation));
			constexpr uint32_t invalidViewAngle = 180;
			Append(bytes, invalidViewAngle);
			constexpr uint8_t perspective = 0;
			Append(bytes, perspective);
			return bytes;
		}

		static std::string BuildVmdWithNonCanonicalInterpolationPadding() {
			std::string bytes(50, '\0');
			constexpr char signature[] = "Vocaloid Motion Data 0002";
			std::memcpy(bytes.data(), signature, sizeof(signature) - 1);
			constexpr uint32_t motionCount = 1;
			Append(bytes, motionCount);
			constexpr char boneName[15]{};
			AppendBytes(bytes, boneName, sizeof(boneName));
			constexpr uint32_t frame = 0;
			Append(bytes, frame);
			Append(bytes, glm::vec3(0));
			Append(bytes, glm::quat(1, 0, 0, 0));
			uint8_t interpolation[64]{};
			interpolation[31] = 255;
			AppendBytes(bytes, reinterpret_cast<const char*>(interpolation), sizeof(interpolation));
			return bytes;
		}

		static std::string BuildVmdWithInvalidShiftJisName() {
			std::string bytes(50, '\0');
			constexpr char signature[] = "Vocaloid Motion Data 0002";
			std::memcpy(bytes.data(), signature, sizeof(signature) - 1);
			constexpr uint32_t motionCount = 1;
			Append(bytes, motionCount);
			char boneName[15]{};
			boneName[0] = static_cast<char>(0x81);
			AppendBytes(bytes, boneName, sizeof(boneName));
			constexpr uint32_t frame = 0;
			Append(bytes, frame);
			Append(bytes, glm::vec3(0));
			Append(bytes, glm::quat(1, 0, 0, 0));
			constexpr uint8_t interpolation[64]{};
			AppendBytes(bytes, reinterpret_cast<const char*>(interpolation), sizeof(interpolation));
			return bytes;
		}
	};

	TEST_F(ParserContractTest, RejectsMissingBoneWithNonZeroWeight) {
		std::istringstream stream(BuildPmxWithMissingWeightedBone());
		PmxParser parser;
		const auto result = parser.Read(stream);
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::InvalidIndex);
		EXPECT_TRUE(parser.GetData().vertices.empty());
	}

	TEST_F(ParserContractTest, RejectsInvalidPmxUtf8Strings) {
		std::istringstream stream(BuildPmxWithInvalidUtf8String());
		PmxParser parser;
		const auto result = parser.Read(stream);
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::InvalidValue);
		EXPECT_TRUE(parser.GetData().info.modelName.empty());
	}

	TEST_F(ParserContractTest, RejectsInvalidCommonToonIndex) {
		std::istringstream stream(BuildPmxWithInvalidCommonToonIndex());
		PmxParser parser;
		const auto result = parser.Read(stream);
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::InvalidValue);
		EXPECT_TRUE(parser.GetData().materials.empty());
	}

	TEST_F(ParserContractTest, AcceptsFiniteZeroLengthVertexNormals) {
		std::istringstream stream(BuildPmxWithZeroLengthNormal());
		PmxParser parser;
		const auto result = parser.Read(stream);
		ASSERT_TRUE(result);
		ASSERT_EQ(parser.GetData().vertices.size(), 1);
		EXPECT_EQ(parser.GetData().vertices.front().normal, glm::vec3(0));
	}

	TEST_F(ParserContractTest, ReadsMinimalVmdWithoutOptionalSections) {
		std::istringstream stream(BuildMinimalVmd());
		VmdParser parser;
		const auto result = parser.Read(stream);
		ASSERT_TRUE(result);
		EXPECT_TRUE(parser.GetData().motions.empty());
		EXPECT_TRUE(parser.GetData().cameras.empty());
	}

	TEST_F(ParserContractTest, RejectsInvalidVmdShowFlag) {
		std::istringstream stream(BuildVmdWithInvalidShowFlag());
		VmdParser parser;
		const auto result = parser.Read(stream);
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::InvalidValue);
		EXPECT_TRUE(parser.GetData().iks.empty());
	}

	TEST_F(ParserContractTest, RejectsNonFiniteVmdMorphWeight) {
		std::istringstream stream(BuildVmdWithNonFiniteMorphWeight());
		VmdParser parser;
		const auto result = parser.Read(stream);
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::InvalidValue);
		EXPECT_TRUE(parser.GetData().morphs.empty());
	}

	TEST_F(ParserContractTest, RejectsInvalidCameraFov) {
		std::istringstream stream(BuildVmdWithInvalidCameraFov());
		VmdParser parser;
		const auto result = parser.Read(stream);
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::InvalidValue);
		EXPECT_TRUE(parser.GetData().cameras.empty());
	}

	TEST_F(ParserContractTest, AcceptsUnusedBoneInterpolationPaddingAbove127) {
		std::istringstream stream(BuildVmdWithNonCanonicalInterpolationPadding());
		VmdParser parser;
		const auto result = parser.Read(stream);
		ASSERT_TRUE(result);
		ASSERT_EQ(parser.GetData().motions.size(), 1);
		EXPECT_EQ(parser.GetData().motions.front().interpolation[31], 255);
	}

	TEST_F(ParserContractTest, RejectsInvalidShiftJisMotionNames) {
		std::istringstream stream(BuildVmdWithInvalidShiftJisName());
		VmdParser parser;
		const auto result = parser.Read(stream);
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ParseErrorCode::InvalidValue);
		EXPECT_TRUE(parser.GetData().motions.empty());
	}

}
