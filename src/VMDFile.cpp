#include "VMDFile.h"

#include "../base/Util.h"

namespace
{
	bool ReadHeader(VMDFile* vmd, File& file) {
		file.Read(vmd->m_header.m_header, sizeof(vmd->m_header.m_header));
		file.Read(vmd->m_header.m_modelName, sizeof(vmd->m_header.m_modelName));
		const std::string header(vmd->m_header.m_header, sizeof(vmd->m_header.m_header));
		if (!(header.starts_with("Vocaloid Motion Data 0002") ||
			header.starts_with("Vocaloid Motion Data")))
			return false;
		return !file.m_badFlag;
	}

	bool ReadMotion(VMDFile* vmd, File& file) {
		uint32_t motionCount = 0;
		if (!file.Read(&motionCount))
			return false;
		vmd->m_motions.resize(motionCount);
		for (auto& [m_boneName,
					m_frame,
					m_translate,
					m_quaternion,
					m_interpolation] : vmd->m_motions) {
			file.Read(m_boneName, sizeof(m_boneName));
			file.Read(&m_frame);
			file.Read(&m_translate);
			file.Read(&m_quaternion);
			file.Read(&m_interpolation);
		}
		return true;
	}

	bool ReadBlendShape(VMDFile* vmd, File& file) {
		uint32_t blendShapeCount = 0;
		if (!file.Read(&blendShapeCount))
			return false;
		vmd->m_morphs.resize(blendShapeCount);
		for (auto& [m_blendShapeName,
					m_frame,
					m_weight]: vmd->m_morphs) {
			file.Read(m_blendShapeName, sizeof(m_blendShapeName));
			file.Read(&m_frame);
			file.Read(&m_weight);
		}
		return !file.m_badFlag;
	}

	bool ReadCamera(VMDFile* vmd, File& file) {
		uint32_t cameraCount = 0;
		if (!file.Read(&cameraCount))
			return false;
		vmd->m_cameras.resize(cameraCount);
		for (auto& [m_frame,
					m_distance,
					m_interest,
					m_rotate,
					m_interpolation,
					m_viewAngle,
					m_isPerspective]: vmd->m_cameras) {
			file.Read(&m_frame);
			file.Read(&m_distance);
			file.Read(&m_interest);
			file.Read(&m_rotate);
			file.Read(&m_interpolation);
			file.Read(&m_viewAngle);
			file.Read(&m_isPerspective);
		}
		return !file.m_badFlag;
	}

	bool ReadLight(VMDFile* vmd, File& file) {
		uint32_t lightCount = 0;
		if (!file.Read(&lightCount))
			return false;
		vmd->m_lights.resize(lightCount);
		for (auto& [m_frame,
					m_color,
					m_position]: vmd->m_lights) {
			file.Read(&m_frame);
			file.Read(&m_color);
			file.Read(&m_position);
		}
		return !file.m_badFlag;
	}

	bool ReadShadow(VMDFile* vmd, File& file) {
		uint32_t shadowCount = 0;
		if (!file.Read(&shadowCount))
			return false;
		vmd->m_shadows.resize(shadowCount);
		for (auto& [m_frame,
					m_shadowType,
					m_distance]: vmd->m_shadows) {
			file.Read(&m_frame);
			file.Read(&m_shadowType);
			file.Read(&m_distance);
		}
		return !file.m_badFlag;
	}

	bool ReadIK(VMDFile* vmd, File& file) {
		uint32_t ikCount = 0;
		if (!file.Read(&ikCount))
			return false;
		vmd->m_iks.resize(ikCount);
		for (auto& [m_frame,
					m_show,
					m_ikInfos]: vmd->m_iks) {
			file.Read(&m_frame);
			file.Read(&m_show);
			uint32_t ikInfoCount = 0;
			if (!file.Read(&ikInfoCount))
				return false;
			m_ikInfos.resize(ikInfoCount);
			for (auto& [m_name,
						m_enable]: m_ikInfos) {
				file.Read(m_name, sizeof(m_name));
				file.Read(&m_enable);
			}
		}
		return !file.m_badFlag;
	}

	bool ReadVMDFile(VMDFile* vmd, File& file) {
		if (!ReadHeader(vmd, file))
			return false;
		if (!ReadMotion(vmd, file))
			return false;
		if (file.Tell() < file.m_fileSize) {
			if (!ReadBlendShape(vmd, file))
				return false;
		}
		if (file.Tell() < file.m_fileSize) {
			if (!ReadCamera(vmd, file))
				return false;
		}
		if (file.Tell() < file.m_fileSize) {
			if (!ReadLight(vmd, file))
				return false;
		}
		if (file.Tell() < file.m_fileSize) {
			if (!ReadShadow(vmd, file))
				return false;
		}
		if (file.Tell() < file.m_fileSize) {
			if (!ReadIK(vmd, file))
				return false;
		}
		return true;
	}
}

bool ReadVMDFile(VMDFile* vmd, const std::filesystem::path& filename) {
	File file;
	if (!file.OpenFile(filename, L"rb"))
		return false;
	return ReadVMDFile(vmd, file);
}
