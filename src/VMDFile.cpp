#include "VMDFile.h"

#include "../base/Util.h"

#include <fstream>

namespace {
	void Read(std::istream& is, void* dst, const std::size_t bytes) {
		is.read(static_cast<char*>(dst), bytes);
	}

	template <class T>
	void Read(std::istream& is, T* dst) {
		Read(is, dst, sizeof(T));
	}

	std::streampos GetFileEnd(std::istream& is) {
		const auto origin = is.tellg();
		is.seekg(0, std::ios::end);
		const auto end = is.tellg();
		is.seekg(origin, std::ios::beg);
		return end;
	}

	bool HasMore(std::istream& is, const std::streampos& end) {
		const auto cur = is.tellg();
		return cur != std::streampos(-1) && cur < end;
	}

	void ReadHeader(VMDFile* vmd, std::istream& is) {
		Read(is, vmd->m_header.m_header, sizeof(vmd->m_header.m_header));
		Read(is, vmd->m_header.m_modelName, sizeof(vmd->m_header.m_modelName));
	}

	void ReadMotion(VMDFile* vmd, std::istream& is) {
		uint32_t motionCount = 0;
		Read(is, &motionCount);
		vmd->m_motions.resize(motionCount);
		for (auto& [m_boneName, m_frame
			     , m_translate, m_quaternion
			     , m_interpolation] : vmd->m_motions) {
			Read(is, m_boneName, sizeof(m_boneName));
			Read(is, &m_frame);
			Read(is, &m_translate);
			Read(is, &m_quaternion);
			Read(is, &m_interpolation);
		}
	}

	void ReadBlendShape(VMDFile* vmd, std::istream& is) {
		uint32_t blendShapeCount = 0;
		Read(is, &blendShapeCount);
		vmd->m_morphs.resize(blendShapeCount);
		for (auto& [m_blendShapeName, m_frame, m_weight]: vmd->m_morphs) {
			Read(is, m_blendShapeName, sizeof(m_blendShapeName));
			Read(is, &m_frame);
			Read(is, &m_weight);
		}
	}

	void ReadCamera(VMDFile* vmd, std::istream& is) {
		uint32_t cameraCount = 0;
		Read(is, &cameraCount);
		vmd->m_cameras.resize(cameraCount);
		for (auto& [m_frame, m_distance, m_interest, m_rotate
			     , m_interpolation, m_viewAngle, m_isPerspective] : vmd->m_cameras) {
			Read(is, &m_frame);
			Read(is, &m_distance);
			Read(is, &m_interest);
			Read(is, &m_rotate);
			Read(is, &m_interpolation);
			Read(is, &m_viewAngle);
			Read(is, &m_isPerspective);
		}
	}

	void ReadLight(VMDFile* vmd, std::istream& is) {
		uint32_t lightCount = 0;
		Read(is, &lightCount);
		vmd->m_lights.resize(lightCount);
		for (auto& [m_frame, m_color, m_position]: vmd->m_lights) {
			Read(is, &m_frame);
			Read(is, &m_color);
			Read(is, &m_position);
		}
	}

	void ReadShadow(VMDFile* vmd, std::istream& is) {
		uint32_t shadowCount = 0;
		Read(is, &shadowCount);
		vmd->m_shadows.resize(shadowCount);
		for (auto& [m_frame, m_shadowType, m_distance]: vmd->m_shadows) {
			Read(is, &m_frame);
			Read(is, &m_shadowType);
			Read(is, &m_distance);
		}
	}

	void ReadIK(VMDFile* vmd, std::istream& is) {
		uint32_t ikCount = 0;
		Read(is, &ikCount);
		vmd->m_iks.resize(ikCount);
		for (auto& [m_frame, m_show, m_ikInfos]: vmd->m_iks) {
			Read(is, &m_frame);
			Read(is, &m_show);
			uint32_t ikInfoCount = 0;
			Read(is, &ikInfoCount);
			m_ikInfos.resize(ikInfoCount);
			for (auto& [m_name, m_enable]: m_ikInfos) {
				Read(is, m_name, sizeof(m_name));
				Read(is, &m_enable);
			}
		}
	}

	void ReadVMDFile(VMDFile* vmd, std::istream& is) {
		const auto end = GetFileEnd(is);
		ReadHeader(vmd, is);
		ReadMotion(vmd, is);
		if (HasMore(is, end))
			ReadBlendShape(vmd, is);
		if (HasMore(is, end))
			ReadCamera(vmd, is);
		if (HasMore(is, end))
			ReadLight(vmd, is);
		if (HasMore(is, end))
			ReadShadow(vmd, is);
		if (HasMore(is, end))
			ReadIK(vmd, is);
	}
}

bool ReadVMDFile(VMDFile* vmd, const std::filesystem::path& filename) {
	std::ifstream is(filename, std::ios::binary);
	if (!is)
		return false;
	ReadVMDFile(vmd, is);
	return true;
}
