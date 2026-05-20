#include "VmdParser.h"

#include <fstream>
#include <iostream>

#include "BinaryReader.h"

namespace Chrivent {
	void VmdParser::ReadHeader(std::istream& is) {
		BinaryReader::Read(is, data.header.header, sizeof(data.header.header));
		BinaryReader::Read(is, data.header.modelName, sizeof(data.header.modelName));
	}

	void VmdParser::ReadMotion(std::istream& is) {
		uint32_t motionCount = 0;
		BinaryReader::Read(is, &motionCount);
		data.motions.resize(motionCount);
		for (auto& [boneName, frame
				 , translate, quaternion
				 , interpolation] : data.motions) {
			BinaryReader::Read(is, boneName, sizeof(boneName));
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &translate);
			BinaryReader::Read(is, &quaternion);
			BinaryReader::Read(is, &interpolation);
				 }
	}

	void VmdParser::ReadBlendShape(std::istream& is) {
		uint32_t blendShapeCount = 0;
		BinaryReader::Read(is, &blendShapeCount);
		data.morphs.resize(blendShapeCount);
		for (auto& [blendShapeName, frame, weight] : data.morphs) {
			BinaryReader::Read(is, blendShapeName, sizeof(blendShapeName));
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &weight);
		}
	}

	void VmdParser::ReadCamera(std::istream& is) {
		uint32_t cameraCount = 0;
		BinaryReader::Read(is, &cameraCount);
		data.cameras.resize(cameraCount);
		for (auto& [frame, distance, interest, rotate
				 , interpolation, viewAngle, isPerspective] : data.cameras) {
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &distance);
			BinaryReader::Read(is, &interest);
			BinaryReader::Read(is, &rotate);
			BinaryReader::Read(is, &interpolation);
			BinaryReader::Read(is, &viewAngle);
			BinaryReader::Read(is, &isPerspective);
				 }
	}

	void VmdParser::ReadLight(std::istream& is) {
		uint32_t lightCount = 0;
		BinaryReader::Read(is, &lightCount);
		data.lights.resize(lightCount);
		for (auto& [frame, color, position] : data.lights) {
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &color);
			BinaryReader::Read(is, &position);
		}
	}

	void VmdParser::ReadShadow(std::istream& is) {
		uint32_t shadowCount = 0;
		BinaryReader::Read(is, &shadowCount);
		data.shadows.resize(shadowCount);
		for (auto& [frame, shadowType, distance] : data.shadows) {
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &shadowType);
			BinaryReader::Read(is, &distance);
		}
	}

	void VmdParser::ReadIk(std::istream& is) {
		uint32_t ikCount = 0;
		BinaryReader::Read(is, &ikCount);
		data.iks.resize(ikCount);
		for (auto& [frame, show, ikInfos] : data.iks) {
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &show);
			uint32_t ikInfoCount = 0;
			BinaryReader::Read(is, &ikInfoCount);
			ikInfos.resize(ikInfoCount);
			for (auto& [name, enable]: ikInfos) {
				BinaryReader::Read(is, name, sizeof(name));
				BinaryReader::Read(is, &enable);
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

	bool VmdParser::ReadFile(const std::filesystem::path& filename) {
		Clear();

		std::ifstream is(filename, std::ios::binary);
		if (!is) {
			std::cerr << "Failed to open VMD file: " << filename.string() << '\n';
			return false;
		}

		const auto fail = [&](const char* stage) {
			if (is)
				return false;
			std::cerr << "Failed to read VMD " << stage << ": " << filename.string() << '\n';
			Clear();
			return true;
		};

		const auto end = BinaryReader::GetFileEnd(is);
		ReadHeader(is);
		if (fail("header")) return false;
		ReadMotion(is);
		if (fail("motions")) return false;
		if (BinaryReader::HasMore(is, end)) {
			ReadBlendShape(is);
			if (fail("morphs")) return false;
		}
		if (BinaryReader::HasMore(is, end)) {
			ReadCamera(is);
			if (fail("cameras")) return false;
		}
		if (BinaryReader::HasMore(is, end)) {
			ReadLight(is);
			if (fail("lights")) return false;
		}
		if (BinaryReader::HasMore(is, end)) {
			ReadShadow(is);
			if (fail("shadows")) return false;
		}
		if (BinaryReader::HasMore(is, end)) {
			ReadIk(is);
			if (fail("IK")) return false;
		}
		return true;
	}
}
