#include "PMXFile.h"

#include "../base/Util.h"

#include <vector>

namespace {
	bool ReadString(const PMXFile* pmx, std::string* val, File& file) {
		uint32_t bufSize;
		if (!file.Read(&bufSize))
			return false;
		if (bufSize > 0) {
			if (pmx->m_header.m_encode == 0) {
				// UTF-16
				std::wstring utf16Str(bufSize / 2, L'\0');
				if (!file.Read(&utf16Str[0], utf16Str.size()))
					return false;
				*val = UnicodeUtil::WStringToUtf8(utf16Str);
			} else if (pmx->m_header.m_encode == 1) {
				// UTF-8
				val->resize(bufSize);
				file.Read(val->data(), bufSize);
			}
		}
		return !file.m_badFlag;
	}

	bool ReadIndex(int32_t* index, const uint8_t indexSize, File& file) {
		switch (indexSize) {
			case 1: {
				uint8_t idx;
				file.Read(&idx);
				if (idx != 0xFF)
					*index = static_cast<int32_t>(idx);
				else
					*index = -1;
			}
			break;
			case 2: {
				uint16_t idx;
				file.Read(&idx);
				if (idx != 0xFFFF)
					*index = static_cast<int32_t>(idx);
				else
					*index = -1;
			}
			break;
			case 4: {
				uint32_t idx;
				file.Read(&idx);
				*index = static_cast<int32_t>(idx);
			}
			break;
			default:
				return false;
		}
		return !file.m_badFlag;
	}

	bool ReadHeader(PMXFile* pmxFile, File& file) {
		auto& [m_magic, m_version, m_dataSize, m_encode, m_addUVNum
			, m_vertexIndexSize, m_textureIndexSize, m_materialIndexSize, m_boneIndexSize
			, m_morphIndexSize, m_rigidbodyIndexSize] = pmxFile->m_header;
		file.Read(m_magic, sizeof(m_magic));
		file.Read(&m_version);
		file.Read(&m_dataSize);
		file.Read(&m_encode);
		file.Read(&m_addUVNum);
		file.Read(&m_vertexIndexSize);
		file.Read(&m_textureIndexSize);
		file.Read(&m_materialIndexSize);
		file.Read(&m_boneIndexSize);
		file.Read(&m_morphIndexSize);
		file.Read(&m_rigidbodyIndexSize);
		return !file.m_badFlag;
	}

	bool ReadInfo(PMXFile* pmx, File& file) {
		auto& [m_modelName, m_englishModelName, m_comment, m_englishComment] = pmx->m_info;
		ReadString(pmx, &m_modelName, file);
		ReadString(pmx, &m_englishModelName, file);
		ReadString(pmx, &m_comment, file);
		ReadString(pmx, &m_englishComment, file);
		return true;
	}

	bool ReadVertex(PMXFile* pmx, File& file) {
		int32_t vertexCount;
		if (!file.Read(&vertexCount))
			return false;
		auto& vertices = pmx->m_vertices;
		vertices.resize(vertexCount);
		for (auto& [m_position, m_normal, m_uv, m_addUV
			     , m_weightType, m_boneIndices, m_boneWeights
			     , m_sdefC, m_sdefR0, m_sdefR1, m_edgeMag] : vertices) {
			file.Read(&m_position);
			file.Read(&m_normal);
			file.Read(&m_uv);
			for (uint8_t i = 0; i < pmx->m_header.m_addUVNum; i++)
				file.Read(&m_addUV[i]);
			file.Read(&m_weightType);
			switch (m_weightType) {
				case PMXVertexWeight::BDEF1:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, file);
					break;
				case PMXVertexWeight::BDEF2:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, file);
					ReadIndex(&m_boneIndices[1], pmx->m_header.m_boneIndexSize, file);
					file.Read(&m_boneWeights[0]);
					break;
				case PMXVertexWeight::BDEF4:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, file);
					ReadIndex(&m_boneIndices[1], pmx->m_header.m_boneIndexSize, file);
					ReadIndex(&m_boneIndices[2], pmx->m_header.m_boneIndexSize, file);
					ReadIndex(&m_boneIndices[3], pmx->m_header.m_boneIndexSize, file);
					file.Read(&m_boneWeights[0]);
					file.Read(&m_boneWeights[1]);
					file.Read(&m_boneWeights[2]);
					file.Read(&m_boneWeights[3]);
					break;
				case PMXVertexWeight::SDEF:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, file);
					ReadIndex(&m_boneIndices[1], pmx->m_header.m_boneIndexSize, file);
					file.Read(&m_boneWeights[0]);
					file.Read(&m_sdefC);
					file.Read(&m_sdefR0);
					file.Read(&m_sdefR1);
					break;
				case PMXVertexWeight::QDEF:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, file);
					ReadIndex(&m_boneIndices[1], pmx->m_header.m_boneIndexSize, file);
					ReadIndex(&m_boneIndices[2], pmx->m_header.m_boneIndexSize, file);
					ReadIndex(&m_boneIndices[3], pmx->m_header.m_boneIndexSize, file);
					file.Read(&m_boneWeights[0]);
					file.Read(&m_boneWeights[1]);
					file.Read(&m_boneWeights[2]);
					file.Read(&m_boneWeights[3]);
					break;
				default:
					return false;
			}
			file.Read(&m_edgeMag);
		}
		return !file.m_badFlag;
	}

	bool ReadFace(PMXFile* pmx, File& file) {
		int32_t faceCount = 0;
		if (!file.Read(&faceCount))
			return false;
		faceCount /= 3;
		pmx->m_faces.resize(faceCount);
		switch (pmx->m_header.m_vertexIndexSize) {
			case 1: {
				std::vector<uint8_t> vertices(faceCount * 3);
				file.Read(vertices.data(), vertices.size());
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
					pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
					pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
				}
			}
			break;
			case 2: {
				std::vector<uint16_t> vertices(faceCount * 3);
				file.Read(vertices.data(), vertices.size());
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
					pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
					pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
				}
			}
			break;
			case 4: {
				std::vector<uint32_t> vertices(faceCount * 3);
				file.Read(vertices.data(), vertices.size());
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
					pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
					pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
				}
			}
			break;
			default:
				return false;
		}
		return !file.m_badFlag;
	}

	bool ReadTexture(PMXFile* pmx, File& file) {
		int32_t texCount = 0;
		if (!file.Read(&texCount))
			return false;
		pmx->m_textures.resize(texCount);
		for (auto& [m_textureName] : pmx->m_textures)
			ReadString(pmx, &m_textureName, file);
		return !file.m_badFlag;
	}

	bool ReadMaterial(PMXFile* pmx, File& file) {
		int32_t matCount = 0;
		if (!file.Read(&matCount))
			return false;
		pmx->m_materials.resize(matCount);
		for (auto& [m_name, m_englishName, m_diffuse, m_specular
			     , m_specularPower, m_ambient, m_drawMode
			     , m_edgeColor, m_edgeSize
			     , m_textureIndex, m_sphereTextureIndex
			     , m_sphereMode, m_toonMode, m_toonTextureIndex
			     , m_memo, m_numFaceVertices] : pmx->m_materials) {
			ReadString(pmx, &m_name, file);
			ReadString(pmx, &m_englishName, file);
			file.Read(&m_diffuse);
			file.Read(&m_specular);
			file.Read(&m_specularPower);
			file.Read(&m_ambient);
			file.Read(&m_drawMode);
			file.Read(&m_edgeColor);
			file.Read(&m_edgeSize);
			ReadIndex(&m_textureIndex, pmx->m_header.m_textureIndexSize, file);
			ReadIndex(&m_sphereTextureIndex, pmx->m_header.m_textureIndexSize, file);
			file.Read(&m_sphereMode);
			file.Read(&m_toonMode);
			if (m_toonMode == PMXToonMode::Separate)
				ReadIndex(&m_toonTextureIndex, pmx->m_header.m_textureIndexSize, file);
			else if (m_toonMode == PMXToonMode::Common) {
				uint8_t toonIndex;
				file.Read(&toonIndex);
				m_toonTextureIndex = static_cast<int32_t>(toonIndex);
			} else
				return false;
			ReadString(pmx, &m_memo, file);
			file.Read(&m_numFaceVertices);
		}
		return !file.m_badFlag;
	}

	bool ReadBone(PMXFile* pmx, File& file) {
		int32_t boneCount;
		if (!file.Read(&boneCount))
			return false;
		pmx->m_bones.resize(boneCount);
		for (auto& [m_name, m_englishName, m_position, m_parentBoneIndex
			     , m_deformDepth, m_boneFlag, m_positionOffset
			     , m_linkBoneIndex, m_appendBoneIndex, m_appendWeight
			     , m_fixedAxis, m_localXAxis, m_localZAxis, m_keyValue
			     , m_ikTargetBoneIndex, m_ikIterationCount
			     , m_ikLimit, m_ikLinks] : pmx->m_bones) {
			ReadString(pmx, &m_name, file);
			ReadString(pmx, &m_englishName, file);
			file.Read(&m_position);
			ReadIndex(&m_parentBoneIndex, pmx->m_header.m_boneIndexSize, file);
			file.Read(&m_deformDepth);
			file.Read(&m_boneFlag);
			if ((static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::TargetShowMode)) == 0)
				file.Read(&m_positionOffset);
			else
				ReadIndex(&m_linkBoneIndex, pmx->m_header.m_boneIndexSize, file);
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::AppendRotate) ||
			    static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::AppendTranslate)) {
				ReadIndex(&m_appendBoneIndex, pmx->m_header.m_boneIndexSize, file);
				file.Read(&m_appendWeight);
			}
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::FixedAxis))
				file.Read(&m_fixedAxis);
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::LocalAxis)) {
				file.Read(&m_localXAxis);
				file.Read(&m_localZAxis);
			}
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::DeformOuterParent))
				file.Read(&m_keyValue);
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::IK)) {
				ReadIndex(&m_ikTargetBoneIndex, pmx->m_header.m_boneIndexSize, file);
				file.Read(&m_ikIterationCount);
				file.Read(&m_ikLimit);
				int32_t linkCount;
				if (!file.Read(&linkCount))
					return false;
				m_ikLinks.resize(linkCount);
				for (auto& [m_ikBoneIndex
					     , m_enableLimit
					     , m_limitMin
					     , m_limitMax] : m_ikLinks) {
					ReadIndex(&m_ikBoneIndex, pmx->m_header.m_boneIndexSize, file);
					file.Read(&m_enableLimit);
					if (m_enableLimit != 0) {
						file.Read(&m_limitMin);
						file.Read(&m_limitMax);
					}
				}
			}
		}
		return !file.m_badFlag;
	}

	bool ReadMorph(PMXFile* pmx, File& file) {
		int32_t morphCount;
		if (!file.Read(&morphCount))
			return false;
		pmx->m_morphs.resize(morphCount);
		for (auto& [m_name, m_englishName, m_controlPanel, m_morphType
			     , m_positionMorph, m_uvMorph, m_boneMorph
			     , m_materialMorph, m_groupMorph
			     , m_flipMorph, m_impulseMorph] : pmx->m_morphs) {
			ReadString(pmx, &m_name, file);
			ReadString(pmx, &m_englishName, file);
			file.Read(&m_controlPanel);
			file.Read(&m_morphType);
			int32_t dataCount;
			if (!file.Read(&dataCount))
				return false;
			if (m_morphType == PMXMorphType::Position) {
				m_positionMorph.resize(dataCount);
				for (auto& [m_vertexIndex, m_position] : m_positionMorph) {
					ReadIndex(&m_vertexIndex, pmx->m_header.m_vertexIndexSize, file);
					file.Read(&m_position);
				}
			} else if (m_morphType == PMXMorphType::UV ||
			           m_morphType == PMXMorphType::AddUV1 ||
			           m_morphType == PMXMorphType::AddUV2 ||
			           m_morphType == PMXMorphType::AddUV3 ||
			           m_morphType == PMXMorphType::AddUV4
			) {
				m_uvMorph.resize(dataCount);
				for (auto& [m_vertexIndex, m_uv] : m_uvMorph) {
					ReadIndex(&m_vertexIndex, pmx->m_header.m_vertexIndexSize, file);
					file.Read(&m_uv);
				}
			} else if (m_morphType == PMXMorphType::Bone) {
				m_boneMorph.resize(dataCount);
				for (auto& [m_boneIndex, m_position, m_quaternion] : m_boneMorph) {
					ReadIndex(&m_boneIndex, pmx->m_header.m_boneIndexSize, file);
					file.Read(&m_position);
					file.Read(&m_quaternion);
				}
			} else if (m_morphType == PMXMorphType::Material) {
				m_materialMorph.resize(dataCount);
				for (auto& [m_materialIndex, m_opType, m_diffuse
					     , m_specular, m_specularPower
					     , m_ambient, m_edgeColor, m_edgeSize
					     , m_textureFactor, m_sphereTextureFactor, m_toonTextureFactor] : m_materialMorph) {
					ReadIndex(&m_materialIndex, pmx->m_header.m_materialIndexSize, file);
					file.Read(&m_opType);
					file.Read(&m_diffuse);
					file.Read(&m_specular);
					file.Read(&m_specularPower);
					file.Read(&m_ambient);
					file.Read(&m_edgeColor);
					file.Read(&m_edgeSize);
					file.Read(&m_textureFactor);
					file.Read(&m_sphereTextureFactor);
					file.Read(&m_toonTextureFactor);
				}
			} else if (m_morphType == PMXMorphType::Group) {
				m_groupMorph.resize(dataCount);
				for (auto& [m_morphIndex, m_weight] : m_groupMorph) {
					ReadIndex(&m_morphIndex, pmx->m_header.m_morphIndexSize, file);
					file.Read(&m_weight);
				}
			} else if (m_morphType == PMXMorphType::Flip) {
				m_flipMorph.resize(dataCount);
				for (auto& [m_morphIndex, m_weight] : m_flipMorph) {
					ReadIndex(&m_morphIndex, pmx->m_header.m_morphIndexSize, file);
					file.Read(&m_weight);
				}
			} else if (m_morphType == PMXMorphType::Impulse) {
				m_impulseMorph.resize(dataCount);
				for (auto& [m_rigidbodyIndex
					     , m_localFlag
					     , m_translateVelocity
					     , m_rotateTorque] : m_impulseMorph) {
					ReadIndex(&m_rigidbodyIndex, pmx->m_header.m_rigidbodyIndexSize, file);
					file.Read(&m_localFlag);
					file.Read(&m_translateVelocity);
					file.Read(&m_rotateTorque);
				}
			}
		}
		return !file.m_badFlag;
	}

	bool ReadDisplayFrame(PMXFile* pmx, File& file) {
		int32_t displayFrameCount;
		if (!file.Read(&displayFrameCount))
			return false;
		pmx->m_displayFrames.resize(displayFrameCount);
		for (auto& [m_name, m_englishName
			     , m_flag, m_targets] : pmx->m_displayFrames) {
			ReadString(pmx, &m_name, file);
			ReadString(pmx, &m_englishName, file);
			file.Read(&m_flag);
			int32_t targetCount;
			if (!file.Read(&targetCount))
				return false;
			m_targets.resize(targetCount);
			for (auto& [m_type, m_index] : m_targets) {
				file.Read(&m_type);
				if (m_type == PMXDisplayFrame::TargetType::BoneIndex)
					ReadIndex(&m_index, pmx->m_header.m_boneIndexSize, file);
				else if (m_type == PMXDisplayFrame::TargetType::MorphIndex)
					ReadIndex(&m_index, pmx->m_header.m_morphIndexSize, file);
				else
					return false;
			}
		}
		return !file.m_badFlag;
	}

	bool ReadRigidbody(PMXFile* pmx, File& file) {
		int32_t rbCount;
		if (!file.Read(&rbCount))
			return false;
		pmx->m_rigidBodies.resize(rbCount);
		for (auto& [m_name, m_englishName, m_boneIndex, m_group, m_collisionGroup
			     , m_shape, m_shapeSize, m_translate, m_rotate, m_mass
			     , m_translateDimmer, m_rotateDimmer
			     , m_repulsion, m_friction, m_op] : pmx->m_rigidBodies) {
			ReadString(pmx, &m_name, file);
			ReadString(pmx, &m_englishName, file);
			ReadIndex(&m_boneIndex, pmx->m_header.m_boneIndexSize, file);
			file.Read(&m_group);
			file.Read(&m_collisionGroup);
			file.Read(&m_shape);
			file.Read(&m_shapeSize);
			file.Read(&m_translate);
			file.Read(&m_rotate);
			file.Read(&m_mass);
			file.Read(&m_translateDimmer);
			file.Read(&m_rotateDimmer);
			file.Read(&m_repulsion);
			file.Read(&m_friction);
			file.Read(&m_op);
		}
		return !file.m_badFlag;
	}

	bool ReadJoint(PMXFile* pmx, File& file) {
		int32_t jointCount;
		if (!file.Read(&jointCount))
			return false;
		pmx->m_joints.resize(jointCount);
		for (auto& [m_name, m_englishName, m_type, m_rigidbodyAIndex, m_rigidbodyBIndex
			     , m_translate, m_rotate, m_translateLowerLimit, m_translateUpperLimit
			     , m_rotateLowerLimit, m_rotateUpperLimit
			     , m_springTranslateFactor, m_springRotateFactor] : pmx->m_joints) {
			ReadString(pmx, &m_name, file);
			ReadString(pmx, &m_englishName, file);
			file.Read(&m_type);
			ReadIndex(&m_rigidbodyAIndex, pmx->m_header.m_rigidbodyIndexSize, file);
			ReadIndex(&m_rigidbodyBIndex, pmx->m_header.m_rigidbodyIndexSize, file);
			file.Read(&m_translate);
			file.Read(&m_rotate);
			file.Read(&m_translateLowerLimit);
			file.Read(&m_translateUpperLimit);
			file.Read(&m_rotateLowerLimit);
			file.Read(&m_rotateUpperLimit);
			file.Read(&m_springTranslateFactor);
			file.Read(&m_springRotateFactor);
		}
		return !file.m_badFlag;
	}

	bool ReadSoftBody(PMXFile* pmx, File& file) {
		int32_t sbCount;
		if (!file.Read(&sbCount))
			return false;
		pmx->m_softbodies.resize(sbCount);
		for (auto& [m_name, m_englishName, m_type, m_materialIndex
			     , m_group, m_collisionGroup, m_flag, m_BLinkLength
			     , m_numClusters, m_totalMass, m_collisionMargin, m_aeroModel
			     , m_VCF, m_DP, m_DG, m_LF, m_PR, m_VC, m_DF, m_MT
			     , m_CHR, m_KHR, m_SHR, m_AHR
			     , m_SRHR_CL, m_SKHR_CL, m_SSHR_CL
			     , m_SR_SPLT_CL, m_SK_SPLT_CL, m_SS_SPLT_CL
			     , m_V_IT, m_P_IT, m_D_IT, m_C_IT
			     , m_LST, m_AST, m_VST
			     , m_anchorRigidBodies, m_pinVertexIndices] : pmx->m_softbodies) {
			ReadString(pmx, &m_name, file);
			ReadString(pmx, &m_englishName, file);
			file.Read(&m_type);
			ReadIndex(&m_materialIndex, pmx->m_header.m_materialIndexSize, file);
			file.Read(&m_group);
			file.Read(&m_collisionGroup);
			file.Read(&m_flag);
			file.Read(&m_BLinkLength);
			file.Read(&m_numClusters);
			file.Read(&m_totalMass);
			file.Read(&m_collisionMargin);
			file.Read(&m_aeroModel);
			file.Read(&m_VCF);
			file.Read(&m_DP);
			file.Read(&m_DG);
			file.Read(&m_LF);
			file.Read(&m_PR);
			file.Read(&m_VC);
			file.Read(&m_DF);
			file.Read(&m_MT);
			file.Read(&m_CHR);
			file.Read(&m_KHR);
			file.Read(&m_SHR);
			file.Read(&m_AHR);
			file.Read(&m_SRHR_CL);
			file.Read(&m_SKHR_CL);
			file.Read(&m_SSHR_CL);
			file.Read(&m_SR_SPLT_CL);
			file.Read(&m_SK_SPLT_CL);
			file.Read(&m_SS_SPLT_CL);
			file.Read(&m_V_IT);
			file.Read(&m_P_IT);
			file.Read(&m_D_IT);
			file.Read(&m_C_IT);
			file.Read(&m_LST);
			file.Read(&m_AST);
			file.Read(&m_VST);
			int32_t arCount;
			if (!file.Read(&arCount))
				return false;
			m_anchorRigidBodies.resize(arCount);
			for (auto& [m_rigidBodyIndex
				     , m_vertexIndex
				     , m_nearMode] : m_anchorRigidBodies) {
				ReadIndex(&m_rigidBodyIndex, pmx->m_header.m_rigidbodyIndexSize, file);
				ReadIndex(&m_vertexIndex, pmx->m_header.m_vertexIndexSize, file);
				file.Read(&m_nearMode);
			}
			int32_t pvCount;
			if (!file.Read(&pvCount))
				return false;
			m_pinVertexIndices.resize(pvCount);
			for (auto& pv : m_pinVertexIndices)
				ReadIndex(&pv, pmx->m_header.m_vertexIndexSize, file);
		}
		return !file.m_badFlag;
	}

	bool ReadPMXFile(PMXFile * pmxFile, File& file) {
		if (!ReadHeader(pmxFile, file))
			return false;
		ReadInfo(pmxFile, file);
		if (!ReadVertex(pmxFile, file))
			return false;
		if (!ReadFace(pmxFile, file))
			return false;
		if (!ReadTexture(pmxFile, file))
			return false;
		if (!ReadMaterial(pmxFile, file))
			return false;
		if (!ReadBone(pmxFile, file))
			return false;
		if (!ReadMorph(pmxFile, file))
			return false;
		if (!ReadDisplayFrame(pmxFile, file))
			return false;
		if (!ReadRigidbody(pmxFile, file))
			return false;
		if (!ReadJoint(pmxFile, file))
			return false;
		if (file.Tell() < file.m_fileSize) {
			if (!ReadSoftBody(pmxFile, file))
				return false;
		}
		return true;
	}
}

bool ReadPMXFile(PMXFile* pmxFile, const char* filename) {
	File file;
	if (!file.OpenFile(filename, "rb"))
		return false;
	if (!ReadPMXFile(pmxFile, file))
		return false;
	return true;
}
