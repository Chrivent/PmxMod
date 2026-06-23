#include "Core/Parser/VmdParser.h"

#include <cstring>
#include <fstream>

namespace Chrivent {
	void VmdParser::ReadHeader(BinaryReader& reader) {
		reader.Read(data.header.header, sizeof(data.header.header));
		reader.Read(data.header.modelName, sizeof(data.header.modelName));
		constexpr char signature[] = "Vocaloid Motion Data 0002";
		if (std::memcmp(data.header.header, signature, sizeof(signature) - 1) != 0)
			reader.Fail(ParseErrorCode::InvalidHeader, "VMD 0002 시그니처가 올바르지 않습니다.");
	}

	void VmdParser::ReadMotion(BinaryReader& reader) {
		uint32_t motionCount = 0;
		if (!reader.ReadCount(motionCount, 111))
			return;
		data.motions.resize(motionCount);
		for (auto& [boneName, frame,
			translate, quaternion,
			interpolation] : data.motions) {
			reader.Read(boneName, sizeof(boneName));
			reader.Read(frame);
			reader.Read(translate);
			reader.Read(quaternion);
			reader.Read(interpolation);
		}
	}

	void VmdParser::ReadBlendShape(BinaryReader& reader) {
		uint32_t blendShapeCount = 0;
		if (!reader.ReadCount(blendShapeCount, 23))
			return;
		data.morphs.resize(blendShapeCount);
		for (auto& [blendShapeName, frame, weight] : data.morphs) {
			reader.Read(blendShapeName, sizeof(blendShapeName));
			reader.Read(frame);
			reader.Read(weight);
		}
	}

	void VmdParser::ReadCamera(BinaryReader& reader) {
		uint32_t cameraCount = 0;
		if (!reader.ReadCount(cameraCount, 61))
			return;
		data.cameras.resize(cameraCount);
		for (auto& [frame, distance, interest, rotate,
			interpolation, viewAngle, isPerspective] : data.cameras) {
			reader.Read(frame);
			reader.Read(distance);
			reader.Read(interest);
			reader.Read(rotate);
			reader.Read(interpolation);
			reader.Read(viewAngle);
			reader.Read(isPerspective);
			if (isPerspective > 1) {
				reader.Fail(ParseErrorCode::InvalidValue, "카메라 원근 플래그가 올바르지 않습니다.");
				return;
			}
		}
	}

	void VmdParser::ReadLight(BinaryReader& reader) {
		uint32_t lightCount = 0;
		if (!reader.ReadCount(lightCount, 28))
			return;
		data.lights.resize(lightCount);
		for (auto& [frame, color, position] : data.lights) {
			reader.Read(frame);
			reader.Read(color);
			reader.Read(position);
		}
	}

	void VmdParser::ReadShadow(BinaryReader& reader) {
		uint32_t shadowCount = 0;
		if (!reader.ReadCount(shadowCount, 9))
			return;
		data.shadows.resize(shadowCount);
		for (auto& [frame, shadowType, distance] : data.shadows) {
			reader.Read(frame);
			reader.Read(shadowType);
			reader.Read(distance);
			if (shadowType > ShadowType::Mode2) {
				reader.Fail(ParseErrorCode::InvalidValue, "그림자 형식 값이 올바르지 않습니다.");
				return;
			}
		}
	}

	void VmdParser::ReadIk(BinaryReader& reader) {
		uint32_t ikCount = 0;
		if (!reader.ReadCount(ikCount, 9))
			return;
		data.iks.resize(ikCount);
		for (auto& [frame, show, ikStates] : data.iks) {
			reader.Read(frame);
			reader.Read(show);
			uint32_t ikInfoCount = 0;
			if (show > 1 || !reader.ReadCount(ikInfoCount, 21))
				return;
			ikStates.resize(ikInfoCount);
			for (auto& [name, enable]: ikStates) {
				reader.Read(name, sizeof(name));
				reader.Read(enable);
				if (enable > 1) {
					reader.Fail(ParseErrorCode::InvalidValue, "IK 활성화 플래그가 올바르지 않습니다.");
					return;
				}
			}
		}
	}

	void VmdParser::Clear() {
		data.header = {};
		data.motions.clear();
		data.morphs.clear();
		data.cameras.clear();
		data.lights.clear();
		data.shadows.clear();
		data.iks.clear();
	}

	std::expected<void, ParseError> VmdParser::ReadFile(const std::filesystem::path& filename) {
		Clear();
		std::ifstream is(filename, std::ios::binary);
		if (!is)
			return std::unexpected(ParseError{
				ParseErrorCode::FileOpen, "file", "VMD 파일을 열 수 없습니다: " + filename.string(), 0
			});
		BinaryReader reader(is);
		const auto ReadSection = [&](const char* section, auto read) {
			reader.SetSection(section);
			read();
			return reader.Result().has_value();
		};
		if (!ReadSection("header", [&] { ReadHeader(reader); }) ||
			!ReadSection("motions", [&] { ReadMotion(reader); })) {
			const auto result = reader.Result();
			Clear();
			return result;
		}
		const auto ReadOptionalSection = [&](const char* section, auto read) {
			return !reader.HasMore() || ReadSection(section, read);
		};
		if (!ReadOptionalSection("morphs", [&] { ReadBlendShape(reader); }) ||
			!ReadOptionalSection("cameras", [&] { ReadCamera(reader); }) ||
			!ReadOptionalSection("lights", [&] { ReadLight(reader); }) ||
			!ReadOptionalSection("shadows", [&] { ReadShadow(reader); }) ||
			!ReadOptionalSection("IK", [&] { ReadIk(reader); })) {
			const auto result = reader.Result();
			Clear();
			return result;
		}
		return reader.Result();
	}
}
