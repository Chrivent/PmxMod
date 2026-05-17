#include "VmdReader.h"

#include <fstream>

#include "BinaryStreamReader.h"

namespace Chrivent {

	void VmdReader::ReadHeader(std::istream& is) {
		BinaryStreamReader::Read(is, header.header, sizeof(header.header));
		BinaryStreamReader::Read(is, header.modelName, sizeof(header.modelName));
	}

	void VmdReader::ReadMotion(std::istream& is) {
		uint32_t motionCount = 0;
		BinaryStreamReader::Read(is, &motionCount);
		motions.resize(motionCount);
		for (auto& [boneName, frame
				 , translate, quaternion
				 , interpolation] : motions) {
			BinaryStreamReader::Read(is, boneName, sizeof(boneName));
			BinaryStreamReader::Read(is, &frame);
			BinaryStreamReader::Read(is, &translate);
			BinaryStreamReader::Read(is, &quaternion);
			BinaryStreamReader::Read(is, &interpolation);
				 }
	}

	void VmdReader::ReadBlendShape(std::istream& is) {
		uint32_t blendShapeCount = 0;
		BinaryStreamReader::Read(is, &blendShapeCount);
		morphs.resize(blendShapeCount);
		for (auto& [blendShapeName, frame, weight] : morphs) {
			BinaryStreamReader::Read(is, blendShapeName, sizeof(blendShapeName));
			BinaryStreamReader::Read(is, &frame);
			BinaryStreamReader::Read(is, &weight);
		}
	}

	void VmdReader::ReadCamera(std::istream& is) {
		uint32_t cameraCount = 0;
		BinaryStreamReader::Read(is, &cameraCount);
		cameras.resize(cameraCount);
		for (auto& [frame, distance, interest, rotate
				 , interpolation, viewAngle, isPerspective] : cameras) {
			BinaryStreamReader::Read(is, &frame);
			BinaryStreamReader::Read(is, &distance);
			BinaryStreamReader::Read(is, &interest);
			BinaryStreamReader::Read(is, &rotate);
			BinaryStreamReader::Read(is, &interpolation);
			BinaryStreamReader::Read(is, &viewAngle);
			BinaryStreamReader::Read(is, &isPerspective);
				 }
	}

	void VmdReader::ReadLight(std::istream& is) {
		uint32_t lightCount = 0;
		BinaryStreamReader::Read(is, &lightCount);
		lights.resize(lightCount);
		for (auto& [frame, color, position] : lights) {
			BinaryStreamReader::Read(is, &frame);
			BinaryStreamReader::Read(is, &color);
			BinaryStreamReader::Read(is, &position);
		}
	}

	void VmdReader::ReadShadow(std::istream& is) {
		uint32_t shadowCount = 0;
		BinaryStreamReader::Read(is, &shadowCount);
		shadows.resize(shadowCount);
		for (auto& [frame, shadowType, distance] : shadows) {
			BinaryStreamReader::Read(is, &frame);
			BinaryStreamReader::Read(is, &shadowType);
			BinaryStreamReader::Read(is, &distance);
		}
	}

	void VmdReader::ReadIk(std::istream& is) {
		uint32_t ikCount = 0;
		BinaryStreamReader::Read(is, &ikCount);
		iks.resize(ikCount);
		for (auto& [frame, show, ikInfos] : iks) {
			BinaryStreamReader::Read(is, &frame);
			BinaryStreamReader::Read(is, &show);
			uint32_t ikInfoCount = 0;
			BinaryStreamReader::Read(is, &ikInfoCount);
			ikInfos.resize(ikInfoCount);
			for (auto& [name, enable]: ikInfos) {
				BinaryStreamReader::Read(is, name, sizeof(name));
				BinaryStreamReader::Read(is, &enable);
			}
		}
	}

	bool VmdReader::ReadFile(const std::filesystem::path& filename) {
		std::ifstream is(filename, std::ios::binary);
		if (!is)
			return false;
		const auto end = BinaryStreamReader::GetFileEnd(is);
		ReadHeader(is);
		ReadMotion(is);
		if (BinaryStreamReader::HasMore(is, end))
			ReadBlendShape(is);
		if (BinaryStreamReader::HasMore(is, end))
			ReadCamera(is);
		if (BinaryStreamReader::HasMore(is, end))
			ReadLight(is);
		if (BinaryStreamReader::HasMore(is, end))
			ReadShadow(is);
		if (BinaryStreamReader::HasMore(is, end))
			ReadIk(is);
		return true;
	}
}
