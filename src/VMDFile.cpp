#include "VMDFile.h"

#include "../base/Util.h"

#include <fstream>

void VMDFile::Read(std::istream& is, void* dst, const std::size_t bytes) {
	is.read(static_cast<char*>(dst), bytes);
}

std::streampos VMDFile::GetFileEnd(std::istream& is) {
	const auto origin = is.tellg();
	is.seekg(0, std::ios::end);
	const auto end = is.tellg();
	is.seekg(origin, std::ios::beg);
	return end;
}

bool VMDFile::HasMore(std::istream& is, const std::streampos& end) {
	const auto cur = is.tellg();
	return cur != std::streampos(-1) && cur < end;
}

void VMDFile::ReadHeader(std::istream& is) {
	Read(is, m_header.m_header, sizeof(m_header.m_header));
	Read(is, m_header.m_modelName, sizeof(m_header.m_modelName));
}

void VMDFile::ReadMotion(std::istream& is) {
	uint32_t motionCount = 0;
	Read(is, &motionCount);
	m_motions.resize(motionCount);
	for (auto& [m_boneName, m_frame
		     , m_translate, m_quaternion
		     , m_interpolation] : m_motions) {
		Read(is, m_boneName, sizeof(m_boneName));
		Read(is, &m_frame);
		Read(is, &m_translate);
		Read(is, &m_quaternion);
		Read(is, &m_interpolation);
	}
}

void VMDFile::ReadBlendShape(std::istream& is) {
	uint32_t blendShapeCount = 0;
	Read(is, &blendShapeCount);
	m_morphs.resize(blendShapeCount);
	for (auto& [m_blendShapeName, m_frame, m_weight] : m_morphs) {
		Read(is, m_blendShapeName, sizeof(m_blendShapeName));
		Read(is, &m_frame);
		Read(is, &m_weight);
	}
}

void VMDFile::ReadCamera(std::istream& is) {
	uint32_t cameraCount = 0;
	Read(is, &cameraCount);
	m_cameras.resize(cameraCount);
	for (auto& [m_frame, m_distance, m_interest, m_rotate
		     , m_interpolation, m_viewAngle, m_isPerspective] : m_cameras) {
		Read(is, &m_frame);
		Read(is, &m_distance);
		Read(is, &m_interest);
		Read(is, &m_rotate);
		Read(is, &m_interpolation);
		Read(is, &m_viewAngle);
		Read(is, &m_isPerspective);
	}
}

void VMDFile::ReadLight(std::istream& is) {
	uint32_t lightCount = 0;
	Read(is, &lightCount);
	m_lights.resize(lightCount);
	for (auto& [m_frame, m_color, m_position] : m_lights) {
		Read(is, &m_frame);
		Read(is, &m_color);
		Read(is, &m_position);
	}
}

void VMDFile::ReadShadow(std::istream& is) {
	uint32_t shadowCount = 0;
	Read(is, &shadowCount);
	m_shadows.resize(shadowCount);
	for (auto& [m_frame, m_shadowType, m_distance] : m_shadows) {
		Read(is, &m_frame);
		Read(is, &m_shadowType);
		Read(is, &m_distance);
	}
}

void VMDFile::ReadIK(std::istream& is) {
	uint32_t ikCount = 0;
	Read(is, &ikCount);
	m_iks.resize(ikCount);
	for (auto& [m_frame, m_show, m_ikInfos] : m_iks) {
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

void VMDFile::ReadVMDFile(std::istream& is) {
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
		ReadIK(is);
}

bool VMDFile::ReadVMDFile(const std::filesystem::path& filename) {
	std::ifstream is(filename, std::ios::binary);
	if (!is)
		return false;
	ReadVMDFile(is);
	return true;
}
