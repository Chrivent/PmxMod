#include "VmdParser.h"

#include <fstream>
#include <iostream>

#include "BinaryReader.h"

namespace Chrivent {
	void VmdParser::ReadHeader(std::istream& is) {
		BinaryReader::Read(is, header.header, sizeof(header.header));
		BinaryReader::Read(is, header.modelName, sizeof(header.modelName));
	}

	void VmdParser::ReadMotion(std::istream& is) {
		uint32_t motionCount = 0;
		BinaryReader::Read(is, &motionCount);
		motions.resize(motionCount);
		for (auto& [boneName, frame
				 , translate, quaternion
				 , interpolation] : motions) {
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
		morphs.resize(blendShapeCount);
		for (auto& [blendShapeName, frame, weight] : morphs) {
			BinaryReader::Read(is, blendShapeName, sizeof(blendShapeName));
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &weight);
		}
	}

	void VmdParser::ReadCamera(std::istream& is) {
		uint32_t cameraCount = 0;
		BinaryReader::Read(is, &cameraCount);
		cameras.resize(cameraCount);
		for (auto& [frame, distance, interest, rotate
				 , interpolation, viewAngle, isPerspective] : cameras) {
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
		lights.resize(lightCount);
		for (auto& [frame, color, position] : lights) {
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &color);
			BinaryReader::Read(is, &position);
		}
	}

	void VmdParser::ReadShadow(std::istream& is) {
		uint32_t shadowCount = 0;
		BinaryReader::Read(is, &shadowCount);
		shadows.resize(shadowCount);
		for (auto& [frame, shadowType, distance] : shadows) {
			BinaryReader::Read(is, &frame);
			BinaryReader::Read(is, &shadowType);
			BinaryReader::Read(is, &distance);
		}
	}

	void VmdParser::ReadIk(std::istream& is) {
		uint32_t ikCount = 0;
		BinaryReader::Read(is, &ikCount);
		iks.resize(ikCount);
		for (auto& [frame, show, ikInfos] : iks) {
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
		header = {};
		motions.clear();
		morphs.clear();
		cameras.clear();
		lights.clear();
		shadows.clear();
		iks.clear();
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
