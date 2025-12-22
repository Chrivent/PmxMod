#include "PMXFile.h"

#include <fstream>

#include "../base/Util.h"

#include <vector>

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

	void ReadString(const PMXFile* pmx, std::string* val, std::istream& is) {
		uint32_t bufSize;
		Read(is, &bufSize);
		if (bufSize > 0) {
			if (pmx->m_header.m_encode == PMXEncode::UTF16) {
				std::wstring utf16Str(bufSize / 2, L'\0');
				Read(is, utf16Str.data(), bufSize);
				*val = UnicodeUtil::WStringToUtf8(utf16Str);
			} else if (pmx->m_header.m_encode == PMXEncode::UTF8) {
				val->resize(bufSize);
				Read(is, val->data(), bufSize);
			}
		}
	}

	void ReadIndex(int32_t* index, const uint8_t indexSize, std::istream& is) {
		switch (indexSize) {
			case 1: {
				uint8_t idx;
				Read(is, &idx);
				if (idx != 0xFF)
					*index = static_cast<int32_t>(idx);
				else
					*index = -1;
			}
			break;
			case 2: {
				uint16_t idx;
				Read(is, &idx);
				if (idx != 0xFFFF)
					*index = static_cast<int32_t>(idx);
				else
					*index = -1;
			}
			break;
			case 4: {
				uint32_t idx;
				Read(is, &idx);
				*index = static_cast<int32_t>(idx);
			}
			break;
			default: ;
		}
	}

	void ReadHeader(PMXFile* pmxFile, std::istream& is) {
		Read(is, pmxFile->m_header.m_magic, sizeof(pmxFile->m_header.m_magic));
		Read(is, &pmxFile->m_header.m_version);
		Read(is, &pmxFile->m_header.m_dataSize);
		Read(is, &pmxFile->m_header.m_encode);
		Read(is, &pmxFile->m_header.m_addUVNum);
		Read(is, &pmxFile->m_header.m_vertexIndexSize);
		Read(is, &pmxFile->m_header.m_textureIndexSize);
		Read(is, &pmxFile->m_header.m_materialIndexSize);
		Read(is, &pmxFile->m_header.m_boneIndexSize);
		Read(is, &pmxFile->m_header.m_morphIndexSize);
		Read(is, &pmxFile->m_header.m_rigidbodyIndexSize);
	}

	void ReadInfo(PMXFile* pmx, std::istream& is) {
		ReadString(pmx, &pmx->m_info.m_modelName, is);
		ReadString(pmx, &pmx->m_info.m_englishModelName, is);
		ReadString(pmx, &pmx->m_info.m_comment, is);
		ReadString(pmx, &pmx->m_info.m_englishComment, is);
	}

	void ReadVertex(PMXFile* pmx, std::istream& is) {
		int32_t vertexCount;
		Read(is, &vertexCount);
		auto& vertices = pmx->m_vertices;
		vertices.resize(vertexCount);
		for (auto& [m_position, m_normal, m_uv, m_addUV
			     , m_weightType, m_boneIndices, m_boneWeights
			     , m_sdefC, m_sdefR0, m_sdefR1, m_edgeMag] : vertices) {
			Read(is, &m_position);
			Read(is, &m_normal);
			Read(is, &m_uv);
			for (uint8_t i = 0; i < pmx->m_header.m_addUVNum; i++)
				Read(is, &m_addUV[i]);
			Read(is, &m_weightType);
			switch (m_weightType) {
				case PMXVertexWeight::BDEF1:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, is);
					break;
				case PMXVertexWeight::BDEF2:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, is);
					ReadIndex(&m_boneIndices[1], pmx->m_header.m_boneIndexSize, is);
					Read(is, &m_boneWeights[0]);
					break;
				case PMXVertexWeight::BDEF4:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, is);
					ReadIndex(&m_boneIndices[1], pmx->m_header.m_boneIndexSize, is);
					ReadIndex(&m_boneIndices[2], pmx->m_header.m_boneIndexSize, is);
					ReadIndex(&m_boneIndices[3], pmx->m_header.m_boneIndexSize, is);
					Read(is, &m_boneWeights[0]);
					Read(is, &m_boneWeights[1]);
					Read(is, &m_boneWeights[2]);
					Read(is, &m_boneWeights[3]);
					break;
				case PMXVertexWeight::SDEF:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, is);
					ReadIndex(&m_boneIndices[1], pmx->m_header.m_boneIndexSize, is);
					Read(is, &m_boneWeights[0]);
					Read(is, &m_sdefC);
					Read(is, &m_sdefR0);
					Read(is, &m_sdefR1);
					break;
				case PMXVertexWeight::QDEF:
					ReadIndex(&m_boneIndices[0], pmx->m_header.m_boneIndexSize, is);
					ReadIndex(&m_boneIndices[1], pmx->m_header.m_boneIndexSize, is);
					ReadIndex(&m_boneIndices[2], pmx->m_header.m_boneIndexSize, is);
					ReadIndex(&m_boneIndices[3], pmx->m_header.m_boneIndexSize, is);
					Read(is, &m_boneWeights[0]);
					Read(is, &m_boneWeights[1]);
					Read(is, &m_boneWeights[2]);
					Read(is, &m_boneWeights[3]);
					break;
				default: ;
			}
			Read(is, &m_edgeMag);
		}
	}

	void ReadFace(PMXFile* pmx, std::istream& is) {
		int32_t faceCount = 0;
		Read(is, &faceCount);
		faceCount /= 3;
		pmx->m_faces.resize(faceCount);
		switch (pmx->m_header.m_vertexIndexSize) {
			case 1: {
				std::vector<uint8_t> vertices(faceCount * 3);
				Read(is, vertices.data(), vertices.size());
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
					pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
					pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
				}
			}
			break;
			case 2: {
				std::vector<uint16_t> vertices(faceCount * 3);
				Read(is, vertices.data(), vertices.size() * sizeof(uint16_t));
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
					pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
					pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
				}
			}
			break;
			case 4: {
				std::vector<uint32_t> vertices(faceCount * 3);
				Read(is, vertices.data(), vertices.size() * sizeof(uint32_t));
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					pmx->m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
					pmx->m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
					pmx->m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
				}
			}
			break;
			default: ;
		}
	}

	void ReadTexture(PMXFile* pmx, std::istream& is) {
		int32_t texCount = 0;
		Read(is, &texCount);
		pmx->m_textures.resize(texCount);
		std::string utf8;
		for (auto& [m_textureName] : pmx->m_textures) {
			ReadString(pmx, &utf8, is);
			const auto* p = reinterpret_cast<const char8_t*>(utf8.data());
			m_textureName = std::filesystem::path(std::u8string(p, p + utf8.size()));
		}
	}

	void ReadMaterial(PMXFile* pmx, std::istream& is) {
		int32_t matCount = 0;
		Read(is, &matCount);
		pmx->m_materials.resize(matCount);
		for (auto& [m_name, m_englishName, m_diffuse, m_specular
			     , m_specularPower, m_ambient, m_drawMode
			     , m_edgeColor, m_edgeSize
			     , m_textureIndex, m_sphereTextureIndex
			     , m_sphereMode, m_toonMode, m_toonTextureIndex
			     , m_memo, m_numFaceVertices] : pmx->m_materials) {
			ReadString(pmx, &m_name, is);
			ReadString(pmx, &m_englishName, is);
			Read(is, &m_diffuse);
			Read(is, &m_specular);
			Read(is, &m_specularPower);
			Read(is, &m_ambient);
			Read(is, &m_drawMode);
			Read(is, &m_edgeColor);
			Read(is, &m_edgeSize);
			ReadIndex(&m_textureIndex, pmx->m_header.m_textureIndexSize, is);
			ReadIndex(&m_sphereTextureIndex, pmx->m_header.m_textureIndexSize, is);
			Read(is, &m_sphereMode);
			Read(is, &m_toonMode);
			if (m_toonMode == PMXToonMode::Separate)
				ReadIndex(&m_toonTextureIndex, pmx->m_header.m_textureIndexSize, is);
			else if (m_toonMode == PMXToonMode::Common) {
				uint8_t toonIndex;
				Read(is, &toonIndex);
				m_toonTextureIndex = static_cast<int32_t>(toonIndex);
			}
			ReadString(pmx, &m_memo, is);
			Read(is, &m_numFaceVertices);
		}
	}

	void ReadBone(PMXFile* pmx, std::istream& is) {
		int32_t boneCount;
		Read(is, &boneCount);
		pmx->m_bones.resize(boneCount);
		for (auto& [m_name, m_englishName, m_position, m_parentBoneIndex
			     , m_deformDepth, m_boneFlag, m_positionOffset
			     , m_linkBoneIndex, m_appendBoneIndex, m_appendWeight
			     , m_fixedAxis, m_localXAxis, m_localZAxis, m_keyValue
			     , m_ikTargetBoneIndex, m_ikIterationCount
			     , m_ikLimit, m_ikLinks] : pmx->m_bones) {
			ReadString(pmx, &m_name, is);
			ReadString(pmx, &m_englishName, is);
			Read(is, &m_position);
			ReadIndex(&m_parentBoneIndex, pmx->m_header.m_boneIndexSize, is);
			Read(is, &m_deformDepth);
			Read(is, &m_boneFlag);
			if ((static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::TargetShowMode)) == 0)
				Read(is, &m_positionOffset);
			else
				ReadIndex(&m_linkBoneIndex, pmx->m_header.m_boneIndexSize, is);
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::AppendRotate) ||
			    static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::AppendTranslate)) {
				ReadIndex(&m_appendBoneIndex, pmx->m_header.m_boneIndexSize, is);
				Read(is, &m_appendWeight);
			}
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::FixedAxis))
				Read(is, &m_fixedAxis);
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::LocalAxis)) {
				Read(is, &m_localXAxis);
				Read(is, &m_localZAxis);
			}
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::DeformOuterParent))
				Read(is, &m_keyValue);
			if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(PMXBoneFlags::IK)) {
				ReadIndex(&m_ikTargetBoneIndex, pmx->m_header.m_boneIndexSize, is);
				Read(is, &m_ikIterationCount);
				Read(is, &m_ikLimit);
				int32_t linkCount;
				Read(is, &linkCount);
				m_ikLinks.resize(linkCount);
				for (auto& [m_ikBoneIndex
					     , m_enableLimit
					     , m_limitMin
					     , m_limitMax] : m_ikLinks) {
					ReadIndex(&m_ikBoneIndex, pmx->m_header.m_boneIndexSize, is);
					Read(is, &m_enableLimit);
					if (m_enableLimit != 0) {
						Read(is, &m_limitMin);
						Read(is, &m_limitMax);
					}
				}
			}
		}
	}

	void ReadMorph(PMXFile* pmx, std::istream& is) {
		int32_t morphCount;
		Read(is, &morphCount);
		pmx->m_morphs.resize(morphCount);
		for (auto& [m_name, m_englishName, m_controlPanel, m_morphType
			     , m_positionMorph, m_uvMorph, m_boneMorph
			     , m_materialMorph, m_groupMorph
			     , m_flipMorph, m_impulseMorph] : pmx->m_morphs) {
			ReadString(pmx, &m_name, is);
			ReadString(pmx, &m_englishName, is);
			Read(is, &m_controlPanel);
			Read(is, &m_morphType);
			int32_t dataCount;
			Read(is, &dataCount);
			if (m_morphType == PMXMorphType::Position) {
				m_positionMorph.resize(dataCount);
				for (auto& [m_vertexIndex, m_position] : m_positionMorph) {
					ReadIndex(&m_vertexIndex, pmx->m_header.m_vertexIndexSize, is);
					Read(is, &m_position);
				}
			} else if (m_morphType == PMXMorphType::UV ||
			           m_morphType == PMXMorphType::AddUV1 ||
			           m_morphType == PMXMorphType::AddUV2 ||
			           m_morphType == PMXMorphType::AddUV3 ||
			           m_morphType == PMXMorphType::AddUV4
			) {
				m_uvMorph.resize(dataCount);
				for (auto& [m_vertexIndex, m_uv] : m_uvMorph) {
					ReadIndex(&m_vertexIndex, pmx->m_header.m_vertexIndexSize, is);
					Read(is, &m_uv);
				}
			} else if (m_morphType == PMXMorphType::Bone) {
				m_boneMorph.resize(dataCount);
				for (auto& [m_boneIndex, m_position, m_quaternion] : m_boneMorph) {
					ReadIndex(&m_boneIndex, pmx->m_header.m_boneIndexSize, is);
					Read(is, &m_position);
					Read(is, &m_quaternion);
				}
			} else if (m_morphType == PMXMorphType::Material) {
				m_materialMorph.resize(dataCount);
				for (auto& [m_materialIndex, m_opType, m_diffuse
					     , m_specular, m_specularPower
					     , m_ambient, m_edgeColor, m_edgeSize
					     , m_textureFactor, m_sphereTextureFactor, m_toonTextureFactor] : m_materialMorph) {
					ReadIndex(&m_materialIndex, pmx->m_header.m_materialIndexSize, is);
					Read(is, &m_opType);
					Read(is, &m_diffuse);
					Read(is, &m_specular);
					Read(is, &m_specularPower);
					Read(is, &m_ambient);
					Read(is, &m_edgeColor);
					Read(is, &m_edgeSize);
					Read(is, &m_textureFactor);
					Read(is, &m_sphereTextureFactor);
					Read(is, &m_toonTextureFactor);
				}
			} else if (m_morphType == PMXMorphType::Group) {
				m_groupMorph.resize(dataCount);
				for (auto& [m_morphIndex, m_weight] : m_groupMorph) {
					ReadIndex(&m_morphIndex, pmx->m_header.m_morphIndexSize, is);
					Read(is, &m_weight);
				}
			} else if (m_morphType == PMXMorphType::Flip) {
				m_flipMorph.resize(dataCount);
				for (auto& [m_morphIndex, m_weight] : m_flipMorph) {
					ReadIndex(&m_morphIndex, pmx->m_header.m_morphIndexSize, is);
					Read(is, &m_weight);
				}
			} else if (m_morphType == PMXMorphType::Impulse) {
				m_impulseMorph.resize(dataCount);
				for (auto& [m_rigidbodyIndex
					     , m_localFlag
					     , m_translateVelocity
					     , m_rotateTorque] : m_impulseMorph) {
					ReadIndex(&m_rigidbodyIndex, pmx->m_header.m_rigidbodyIndexSize, is);
					Read(is, &m_localFlag);
					Read(is, &m_translateVelocity);
					Read(is, &m_rotateTorque);
				}
			}
		}
	}

	void ReadDisplayFrame(PMXFile* pmx, std::istream& is) {
		int32_t displayFrameCount;
		Read(is, &displayFrameCount);
		pmx->m_displayFrames.resize(displayFrameCount);
		for (auto& [m_name, m_englishName
			     , m_flag, m_targets] : pmx->m_displayFrames) {
			ReadString(pmx, &m_name, is);
			ReadString(pmx, &m_englishName, is);
			Read(is, &m_flag);
			int32_t targetCount;
			Read(is, &targetCount);
			m_targets.resize(targetCount);
			for (auto& [m_type, m_index] : m_targets) {
				Read(is, &m_type);
				if (m_type == PMXDisplayFrame::TargetType::BoneIndex)
					ReadIndex(&m_index, pmx->m_header.m_boneIndexSize, is);
				else if (m_type == PMXDisplayFrame::TargetType::MorphIndex)
					ReadIndex(&m_index, pmx->m_header.m_morphIndexSize, is);
			}
		}
	}

	void ReadRigidbody(PMXFile* pmx, std::istream& is) {
		int32_t rbCount;
		Read(is, &rbCount);
		pmx->m_rigidBodies.resize(rbCount);
		for (auto& [m_name, m_englishName, m_boneIndex, m_group, m_collisionGroup
			     , m_shape, m_shapeSize, m_translate, m_rotate, m_mass
			     , m_translateDimmer, m_rotateDimmer
			     , m_repulsion, m_friction, m_op] : pmx->m_rigidBodies) {
			ReadString(pmx, &m_name, is);
			ReadString(pmx, &m_englishName, is);
			ReadIndex(&m_boneIndex, pmx->m_header.m_boneIndexSize, is);
			Read(is, &m_group);
			Read(is, &m_collisionGroup);
			Read(is, &m_shape);
			Read(is, &m_shapeSize);
			Read(is, &m_translate);
			Read(is, &m_rotate);
			Read(is, &m_mass);
			Read(is, &m_translateDimmer);
			Read(is, &m_rotateDimmer);
			Read(is, &m_repulsion);
			Read(is, &m_friction);
			Read(is, &m_op);
		}
	}

	void ReadJoint(PMXFile* pmx, std::istream& is) {
		int32_t jointCount;
		Read(is, &jointCount);
		pmx->m_joints.resize(jointCount);
		for (auto& [m_name, m_englishName, m_type, m_rigidbodyAIndex, m_rigidbodyBIndex
			     , m_translate, m_rotate, m_translateLowerLimit, m_translateUpperLimit
			     , m_rotateLowerLimit, m_rotateUpperLimit
			     , m_springTranslateFactor, m_springRotateFactor] : pmx->m_joints) {
			ReadString(pmx, &m_name, is);
			ReadString(pmx, &m_englishName, is);
			Read(is, &m_type);
			ReadIndex(&m_rigidbodyAIndex, pmx->m_header.m_rigidbodyIndexSize, is);
			ReadIndex(&m_rigidbodyBIndex, pmx->m_header.m_rigidbodyIndexSize, is);
			Read(is, &m_translate);
			Read(is, &m_rotate);
			Read(is, &m_translateLowerLimit);
			Read(is, &m_translateUpperLimit);
			Read(is, &m_rotateLowerLimit);
			Read(is, &m_rotateUpperLimit);
			Read(is, &m_springTranslateFactor);
			Read(is, &m_springRotateFactor);
		}
	}

	void ReadSoftBody(PMXFile* pmx, std::istream& is) {
		int32_t sbCount;
		Read(is, &sbCount);
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
			ReadString(pmx, &m_name, is);
			ReadString(pmx, &m_englishName, is);
			Read(is, &m_type);
			ReadIndex(&m_materialIndex, pmx->m_header.m_materialIndexSize, is);
			Read(is, &m_group);
			Read(is, &m_collisionGroup);
			Read(is, &m_flag);
			Read(is, &m_BLinkLength);
			Read(is, &m_numClusters);
			Read(is, &m_totalMass);
			Read(is, &m_collisionMargin);
			Read(is, &m_aeroModel);
			Read(is, &m_VCF);
			Read(is, &m_DP);
			Read(is, &m_DG);
			Read(is, &m_LF);
			Read(is, &m_PR);
			Read(is, &m_VC);
			Read(is, &m_DF);
			Read(is, &m_MT);
			Read(is, &m_CHR);
			Read(is, &m_KHR);
			Read(is, &m_SHR);
			Read(is, &m_AHR);
			Read(is, &m_SRHR_CL);
			Read(is, &m_SKHR_CL);
			Read(is, &m_SSHR_CL);
			Read(is, &m_SR_SPLT_CL);
			Read(is, &m_SK_SPLT_CL);
			Read(is, &m_SS_SPLT_CL);
			Read(is, &m_V_IT);
			Read(is, &m_P_IT);
			Read(is, &m_D_IT);
			Read(is, &m_C_IT);
			Read(is, &m_LST);
			Read(is, &m_AST);
			Read(is, &m_VST);
			int32_t arCount;
			Read(is, &arCount);
			m_anchorRigidBodies.resize(arCount);
			for (auto& [m_rigidBodyIndex
				     , m_vertexIndex
				     , m_nearMode] : m_anchorRigidBodies) {
				ReadIndex(&m_rigidBodyIndex, pmx->m_header.m_rigidbodyIndexSize, is);
				ReadIndex(&m_vertexIndex, pmx->m_header.m_vertexIndexSize, is);
				Read(is, &m_nearMode);
			}
			int32_t pvCount;
			Read(is, &pvCount);
			m_pinVertexIndices.resize(pvCount);
			for (auto& pv : m_pinVertexIndices)
				ReadIndex(&pv, pmx->m_header.m_vertexIndexSize, is);
		}
	}

	void ReadPMXFile(PMXFile* pmxFile, std::istream& is) {
		const auto end = GetFileEnd(is);
		ReadHeader(pmxFile, is);
		ReadInfo(pmxFile, is);
		ReadVertex(pmxFile, is);
		ReadFace(pmxFile, is);
		ReadTexture(pmxFile, is);
		ReadMaterial(pmxFile, is);
		ReadBone(pmxFile, is);
		ReadMorph(pmxFile, is);
		ReadDisplayFrame(pmxFile, is);
		ReadRigidbody(pmxFile, is);
		ReadJoint(pmxFile, is);
		if (HasMore(is, end))
			ReadSoftBody(pmxFile, is);
	}
}

bool ReadPMXFile(PMXFile* pmxFile, const std::filesystem::path& filename) {
	std::ifstream is(filename, std::ios::binary);
	if (!is)
		return false;
	ReadPMXFile(pmxFile, is);
	return true;
}
