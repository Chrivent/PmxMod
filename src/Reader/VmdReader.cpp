#include "VmdReader.h"

#include <fstream>

#include "ReaderHelper.h"

namespace Chrivent {
	using namespace ReaderHelper;

	void VmdReader::ReadHeader(std::istream& is) {
		Read(is, header.header, sizeof(header.header));
		Read(is, header.modelName, sizeof(header.modelName));
	}

	void VmdReader::ReadMotion(std::istream& is) {
		uint32_t motionCount = 0;
		Read(is, &motionCount);
		motions.resize(motionCount);
		for (auto& [boneName, frame
				 , translate, quaternion
				 , interpolation] : motions) {
			Read(is, boneName, sizeof(boneName));
			Read(is, &frame);
			Read(is, &translate);
			Read(is, &quaternion);
			Read(is, &interpolation);
				 }
	}

	void VmdReader::ReadBlendShape(std::istream& is) {
		uint32_t blendShapeCount = 0;
		Read(is, &blendShapeCount);
		morphs.resize(blendShapeCount);
		for (auto& [blendShapeName, frame, weight] : morphs) {
			Read(is, blendShapeName, sizeof(blendShapeName));
			Read(is, &frame);
			Read(is, &weight);
		}
	}

	void VmdReader::ReadCamera(std::istream& is) {
		uint32_t cameraCount = 0;
		Read(is, &cameraCount);
		cameras.resize(cameraCount);
		for (auto& [frame, distance, interest, rotate
				 , interpolation, viewAngle, isPerspective] : cameras) {
			Read(is, &frame);
			Read(is, &distance);
			Read(is, &interest);
			Read(is, &rotate);
			Read(is, &interpolation);
			Read(is, &viewAngle);
			Read(is, &isPerspective);
				 }
	}

	void VmdReader::ReadLight(std::istream& is) {
		uint32_t lightCount = 0;
		Read(is, &lightCount);
		lights.resize(lightCount);
		for (auto& [frame, color, position] : lights) {
			Read(is, &frame);
			Read(is, &color);
			Read(is, &position);
		}
	}

	void VmdReader::ReadShadow(std::istream& is) {
		uint32_t shadowCount = 0;
		Read(is, &shadowCount);
		shadows.resize(shadowCount);
		for (auto& [frame, shadowType, distance] : shadows) {
			Read(is, &frame);
			Read(is, &shadowType);
			Read(is, &distance);
		}
	}

	void VmdReader::ReadIk(std::istream& is) {
		uint32_t ikCount = 0;
		Read(is, &ikCount);
		iks.resize(ikCount);
		for (auto& [frame, show, ikInfos] : iks) {
			Read(is, &frame);
			Read(is, &show);
			uint32_t ikInfoCount = 0;
			Read(is, &ikInfoCount);
			ikInfos.resize(ikInfoCount);
			for (auto& [name, enable]: ikInfos) {
				Read(is, name, sizeof(name));
				Read(is, &enable);
			}
		}
	}

	bool VmdReader::ReadFile(const std::filesystem::path& filename) {
		std::ifstream is(filename, std::ios::binary);
		if (!is)
			return false;
		const auto end = GetFileEnd(is);
		ReadHeader(is);
		ReadMotion(is);
		if (HasMore(is, end))
			ReadBlendShape(is);
		if (HasMore(is, end))
			ReadCamera(is);
		if (HasMore(is, end))
			ReadLight(is);
		if (HasMore(is, end))
			ReadShadow(is);
		if (HasMore(is, end))
			ReadIk(is);
		return true;
	}
}
