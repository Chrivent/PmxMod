#include "VMDFile.h"

#include "../base/File.h"

namespace saba
{
	namespace
	{
		template <typename T>
		bool Read(T* val, File& file)
		{
			return file.Read(val);
		}

		bool ReadHeader(VMDFile* vmd, File& file)
		{
			Read(&vmd->m_header.m_header, file);
			Read(&vmd->m_header.m_modelName, file);

			if (vmd->m_header.m_header.ToString() != "Vocaloid Motion Data 0002" &&
				vmd->m_header.m_header.ToString() != "Vocaloid Motion Data"
				)
			{
				return false;
			}

			return !file.m_badFlag;
		}

		bool ReadMotion(VMDFile* vmd, File& file)
		{
			uint32_t motionCount = 0;
			if (!Read(&motionCount, file))
			{
				return false;
			}

			vmd->m_motions.resize(motionCount);
			for (auto& [m_boneName, m_frame, m_translate, m_quaternion, m_interpolation] : vmd->m_motions)
			{
				Read(&m_boneName, file);
				Read(&m_frame, file);
				Read(&m_translate, file);
				Read(&m_quaternion, file);
				Read(&m_interpolation, file);
			}

			return true;
		}

		bool ReadBlendShape(VMDFile* vmd, File& file)
		{
			uint32_t blendShapeCount = 0;
			if (!Read(&blendShapeCount, file))
			{
				return false;
			}

			vmd->m_morphs.resize(blendShapeCount);
			for (auto& morph : vmd->m_morphs)
			{
				Read(&morph.m_blendShapeName, file);
				Read(&morph.m_frame, file);
				Read(&morph.m_weight, file);
			}

			return !file.m_badFlag;
		}

		bool ReadCamera(VMDFile* vmd, File& file)
		{
			uint32_t cameraCount = 0;
			if (!Read(&cameraCount, file))
			{
				return false;
			}

			vmd->m_cameras.resize(cameraCount);
			for (auto& camera : vmd->m_cameras)
			{
				Read(&camera.m_frame, file);
				Read(&camera.m_distance, file);
				Read(&camera.m_interest, file);
				Read(&camera.m_rotate, file);
				Read(&camera.m_interpolation, file);
				Read(&camera.m_viewAngle, file);
				Read(&camera.m_isPerspective, file);
			}

			return !file.m_badFlag;
		}

		bool ReadLight(VMDFile* vmd, File& file)
		{
			uint32_t lightCount = 0;
			if (!Read(&lightCount, file))
			{
				return false;
			}

			vmd->m_lights.resize(lightCount);
			for (auto& light : vmd->m_lights)
			{
				Read(&light.m_frame, file);
				Read(&light.m_color, file);
				Read(&light.m_position, file);
			}

			return !file.m_badFlag;
		}

		bool ReadShadow(VMDFile* vmd, File& file)
		{
			uint32_t shadowCount = 0;
			if (!Read(&shadowCount, file))
			{
				return false;
			}

			vmd->m_shadows.resize(shadowCount);
			for (auto& shadow : vmd->m_shadows)
			{
				Read(&shadow.m_frame, file);
				Read(&shadow.m_shadowType, file);
				Read(&shadow.m_distance, file);
			}

			return !file.m_badFlag;
		}

		bool ReadIK(VMDFile* vmd, File& file)
		{
			uint32_t ikCount = 0;
			if (!Read(&ikCount, file))
			{
				return false;
			}

			vmd->m_iks.resize(ikCount);
			for (auto& ik : vmd->m_iks)
			{
				Read(&ik.m_frame, file);
				Read(&ik.m_show, file);
				uint32_t ikInfoCount = 0;
				if (!Read(&ikInfoCount, file))
				{
					return false;
				}
				ik.m_ikInfos.resize(ikInfoCount);
				for (auto& ikInfo : ik.m_ikInfos)
				{
					Read(&ikInfo.m_name, file);
					Read(&ikInfo.m_enable, file);
				}
			}

			return !file.m_badFlag;
		}

		bool ReadVMDFile(VMDFile* vmd, File& file)
		{
			if (!ReadHeader(vmd, file))
			{
				return false;
			}

			if (!ReadMotion(vmd, file))
			{
				return false;
			}

			if (file.Tell() < file.m_fileSize)
			{
				if (!ReadBlendShape(vmd, file))
				{
					return false;
				}
			}

			if (file.Tell() < file.m_fileSize)
			{
				if (!ReadCamera(vmd, file))
				{
					return false;
				}
			}

			if (file.Tell() < file.m_fileSize)
			{
				if (!ReadLight(vmd, file))
				{
					return false;
				}
			}

			if (file.Tell() < file.m_fileSize)
			{
				if (!ReadShadow(vmd, file))
				{
					return false;
				}
			}

			if (file.Tell() < file.m_fileSize)
			{
				if (!ReadIK(vmd, file))
				{
					return false;
				}
			}

			return true;
		}
	}

	bool ReadVMDFile(VMDFile * vmd, const char * filename)
	{
		File file;
		if (!file.OpenFile(filename, "rb"))
		{
			return false;
		}

		return ReadVMDFile(vmd, file);
	}

}
