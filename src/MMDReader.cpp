#include "MMDReader.h"

#include "MMDUtil.h"

#include <vector>
#include <fstream>

void MMDReader::Read(std::istream& is, void* dst, const std::size_t bytes) {
	is.read(static_cast<char*>(dst), static_cast<long long>(bytes));
}

std::streampos MMDReader::GetFileEnd(std::istream& is) {
	const auto origin = is.tellg();
	is.seekg(0, std::ios::end);
	const auto end = is.tellg();
	is.seekg(origin, std::ios::beg);
	return end;
}

bool MMDReader::HasMore(std::istream& is, const std::streampos& end) {
	const auto cur = is.tellg();
	return cur != std::streampos(-1) && cur < end;
}

void PMXReader::ReadString(std::istream& is, std::string* val) const {
	uint32_t bufSize;
	Read(is, &bufSize);
	if (bufSize > 0) {
		if (m_header.m_encodeType == EncodeType::UTF16) {
			std::wstring utf16Str(bufSize / 2, L'\0');
			Read(is, utf16Str.data(), bufSize);
			*val = UnicodeUtil::WStringToUtf8(utf16Str);
		} else if (m_header.m_encodeType == EncodeType::UTF8) {
			val->resize(bufSize);
			Read(is, val->data(), bufSize);
		}
	}
}

void PMXReader::ReadIndex(std::istream& is, int32_t* index, uint8_t indexSize) {
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

void PMXReader::ReadHeader(std::istream& is) {
	Read(is, m_header.m_magic, sizeof(m_header.m_magic));
	Read(is, &m_header.m_version);
	Read(is, &m_header.m_dataSize);
	Read(is, &m_header.m_encodeType);
	Read(is, &m_header.m_addUVNum);
	Read(is, &m_header.m_vertexIndexSize);
	Read(is, &m_header.m_textureIndexSize);
	Read(is, &m_header.m_materialIndexSize);
	Read(is, &m_header.m_boneIndexSize);
	Read(is, &m_header.m_morphIndexSize);
	Read(is, &m_header.m_rigidbodyIndexSize);
}

void PMXReader::ReadInfo(std::istream& is) {
	ReadString(is, &m_info.m_modelName);
	ReadString(is, &m_info.m_englishModelName);
	ReadString(is, &m_info.m_comment);
	ReadString(is, &m_info.m_englishComment);
}

void PMXReader::ReadVertex(std::istream& is) {
	int32_t vertexCount;
	Read(is, &vertexCount);
	m_vertices.resize(vertexCount);
	for (auto& [m_position, m_normal, m_uv, m_addUV
		     , m_weightType, m_boneIndices, m_boneWeights
		     , m_sdefC, m_sdefR0, m_sdefR1, m_edgeMag] : m_vertices) {
		Read(is, &m_position);
		Read(is, &m_normal);
		Read(is, &m_uv);
		for (uint8_t i = 0; i < m_header.m_addUVNum; i++)
			Read(is, &m_addUV[i]);
		Read(is, &m_weightType);
		switch (m_weightType) {
			case WeightType::BDEF1:
				ReadIndex(is, &m_boneIndices[0], m_header.m_boneIndexSize);
				break;
			case WeightType::BDEF2:
				ReadIndex(is, &m_boneIndices[0], m_header.m_boneIndexSize);
				ReadIndex(is, &m_boneIndices[1], m_header.m_boneIndexSize);
				Read(is, &m_boneWeights[0]);
				break;
			case WeightType::BDEF4:
				ReadIndex(is, &m_boneIndices[0], m_header.m_boneIndexSize);
				ReadIndex(is, &m_boneIndices[1], m_header.m_boneIndexSize);
				ReadIndex(is, &m_boneIndices[2], m_header.m_boneIndexSize);
				ReadIndex(is, &m_boneIndices[3], m_header.m_boneIndexSize);
				Read(is, &m_boneWeights[0]);
				Read(is, &m_boneWeights[1]);
				Read(is, &m_boneWeights[2]);
				Read(is, &m_boneWeights[3]);
				break;
			case WeightType::SDEF:
				ReadIndex(is, &m_boneIndices[0], m_header.m_boneIndexSize);
				ReadIndex(is, &m_boneIndices[1], m_header.m_boneIndexSize);
				Read(is, &m_boneWeights[0]);
				Read(is, &m_sdefC);
				Read(is, &m_sdefR0);
				Read(is, &m_sdefR1);
				break;
			case WeightType::QDEF:
				ReadIndex(is, &m_boneIndices[0], m_header.m_boneIndexSize);
				ReadIndex(is, &m_boneIndices[1], m_header.m_boneIndexSize);
				ReadIndex(is, &m_boneIndices[2], m_header.m_boneIndexSize);
				ReadIndex(is, &m_boneIndices[3], m_header.m_boneIndexSize);
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

void PMXReader::ReadFace(std::istream& is) {
	int32_t faceCount = 0;
	Read(is, &faceCount);
	faceCount /= 3;
	m_faces.resize(faceCount);
	switch (m_header.m_vertexIndexSize) {
		case 1: {
			std::vector<uint8_t> vertices(faceCount * 3);
			Read(is, vertices.data(), vertices.size());
			for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
				m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
				m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
				m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
			}
		}
		break;
		case 2: {
			std::vector<uint16_t> vertices(faceCount * 3);
			Read(is, vertices.data(), vertices.size() * sizeof(uint16_t));
			for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
				m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
				m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
				m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
			}
		}
		break;
		case 4: {
			std::vector<uint32_t> vertices(faceCount * 3);
			Read(is, vertices.data(), vertices.size() * sizeof(uint32_t));
			for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
				m_faces[faceIdx].m_vertices[0] = vertices[faceIdx * 3 + 0];
				m_faces[faceIdx].m_vertices[1] = vertices[faceIdx * 3 + 1];
				m_faces[faceIdx].m_vertices[2] = vertices[faceIdx * 3 + 2];
			}
		}
		break;
		default: ;
	}
}

void PMXReader::ReadTexture(std::istream& is) {
	int32_t texCount = 0;
	Read(is, &texCount);
	m_textures.resize(texCount);
	std::string utf8;
	for (auto& [m_textureName] : m_textures) {
		ReadString(is, &utf8);
		const auto* p = reinterpret_cast<const char8_t*>(utf8.data());
		m_textureName = std::filesystem::path(std::u8string(p, p + utf8.size()));
	}
}

void PMXReader::ReadMaterial(std::istream& is) {
	int32_t matCount = 0;
	Read(is, &matCount);
	m_materials.resize(matCount);
	for (auto& [m_name, m_englishName, m_diffuse, m_specular
		     , m_specularPower, m_ambient, m_drawMode
		     , m_edgeColor, m_edgeSize
		     , m_textureIndex, m_sphereTextureIndex
		     , m_sphereMode, m_toonMode, m_toonTextureIndex
		     , m_memo, m_numFaceVertices] : m_materials) {
		ReadString(is, &m_name);
		ReadString(is, &m_englishName);
		Read(is, &m_diffuse);
		Read(is, &m_specular);
		Read(is, &m_specularPower);
		Read(is, &m_ambient);
		Read(is, &m_drawMode);
		Read(is, &m_edgeColor);
		Read(is, &m_edgeSize);
		ReadIndex(is, &m_textureIndex, m_header.m_textureIndexSize);
		ReadIndex(is, &m_sphereTextureIndex, m_header.m_textureIndexSize);
		Read(is, &m_sphereMode);
		Read(is, &m_toonMode);
		if (m_toonMode == ToonMode::Separate)
			ReadIndex(is, &m_toonTextureIndex, m_header.m_textureIndexSize);
		else if (m_toonMode == ToonMode::Common) {
			uint8_t toonIndex;
			Read(is, &toonIndex);
			m_toonTextureIndex = static_cast<int32_t>(toonIndex);
		}
		ReadString(is, &m_memo);
		Read(is, &m_numFaceVertices);
	}
}

void PMXReader::ReadBone(std::istream& is) {
	int32_t boneCount;
	Read(is, &boneCount);
	m_bones.resize(boneCount);
	for (auto& [m_name, m_englishName, m_position, m_parentBoneIndex
		     , m_deformDepth, m_boneFlag, m_positionOffset
		     , m_linkBoneIndex, m_appendBoneIndex, m_appendWeight
		     , m_fixedAxis, m_localXAxis, m_localZAxis, m_keyValue
		     , m_ikTargetBoneIndex, m_ikIterationCount
		     , m_ikLimit, m_ikLinks] : m_bones) {
		ReadString(is, &m_name);
		ReadString(is, &m_englishName);
		Read(is, &m_position);
		ReadIndex(is, &m_parentBoneIndex, m_header.m_boneIndexSize);
		Read(is, &m_deformDepth);
		Read(is, &m_boneFlag);
		if ((static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(BoneFlags::TargetShowMode)) == 0)
			Read(is, &m_positionOffset);
		else
			ReadIndex(is, &m_linkBoneIndex, m_header.m_boneIndexSize);
		if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(BoneFlags::AppendRotate) ||
		    static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(BoneFlags::AppendTranslate)) {
			ReadIndex(is, &m_appendBoneIndex, m_header.m_boneIndexSize);
			Read(is, &m_appendWeight);
		}
		if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(BoneFlags::FixedAxis))
			Read(is, &m_fixedAxis);
		if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(BoneFlags::LocalAxis)) {
			Read(is, &m_localXAxis);
			Read(is, &m_localZAxis);
		}
		if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(BoneFlags::DeformOuterParent))
			Read(is, &m_keyValue);
		if (static_cast<uint16_t>(m_boneFlag) & static_cast<uint16_t>(BoneFlags::IK)) {
			ReadIndex(is, &m_ikTargetBoneIndex, m_header.m_boneIndexSize);
			Read(is, &m_ikIterationCount);
			Read(is, &m_ikLimit);
			int32_t linkCount;
			Read(is, &linkCount);
			m_ikLinks.resize(linkCount);
			for (auto& [m_ikBoneIndex
				     , m_enableLimit
				     , m_limitMin
				     , m_limitMax] : m_ikLinks) {
				ReadIndex(is, &m_ikBoneIndex, m_header.m_boneIndexSize);
				Read(is, &m_enableLimit);
				if (m_enableLimit != 0) {
					Read(is, &m_limitMin);
					Read(is, &m_limitMax);
				}
			}
		}
	}
}

void PMXReader::ReadMorph(std::istream& is) {
	int32_t morphCount;
	Read(is, &morphCount);
	m_morphs.resize(morphCount);
	for (auto& [m_name, m_englishName, m_controlPanel, m_morphType
		     , m_positionMorph, m_uvMorph, m_boneMorph
		     , m_materialMorph, m_groupMorph
		     , m_flipMorph, m_impulseMorph] : m_morphs) {
		ReadString(is, &m_name);
		ReadString(is, &m_englishName);
		Read(is, &m_controlPanel);
		Read(is, &m_morphType);
		int32_t dataCount;
		Read(is, &dataCount);
		if (m_morphType == MorphType::Position) {
			m_positionMorph.resize(dataCount);
			for (auto& [m_vertexIndex, m_position] : m_positionMorph) {
				ReadIndex(is, &m_vertexIndex, m_header.m_vertexIndexSize);
				Read(is, &m_position);
			}
		} else if (m_morphType == MorphType::UV ||
		           m_morphType == MorphType::AddUV1 ||
		           m_morphType == MorphType::AddUV2 ||
		           m_morphType == MorphType::AddUV3 ||
		           m_morphType == MorphType::AddUV4
		) {
			m_uvMorph.resize(dataCount);
			for (auto& [m_vertexIndex, m_uv] : m_uvMorph) {
				ReadIndex(is, &m_vertexIndex, m_header.m_vertexIndexSize);
				Read(is, &m_uv);
			}
		} else if (m_morphType == MorphType::Bone) {
			m_boneMorph.resize(dataCount);
			for (auto& [m_boneIndex, m_position, m_quaternion] : m_boneMorph) {
				ReadIndex(is, &m_boneIndex, m_header.m_boneIndexSize);
				Read(is, &m_position);
				Read(is, &m_quaternion);
			}
		} else if (m_morphType == MorphType::Material) {
			m_materialMorph.resize(dataCount);
			for (auto& [m_materialIndex, m_opType, m_diffuse
				     , m_specular, m_specularPower
				     , m_ambient, m_edgeColor, m_edgeSize
				     , m_textureFactor, m_sphereTextureFactor, m_toonTextureFactor] : m_materialMorph) {
				ReadIndex(is, &m_materialIndex, m_header.m_materialIndexSize);
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
		} else if (m_morphType == MorphType::Group) {
			m_groupMorph.resize(dataCount);
			for (auto& [m_morphIndex, m_weight] : m_groupMorph) {
				ReadIndex(is, &m_morphIndex, m_header.m_morphIndexSize);
				Read(is, &m_weight);
			}
		} else if (m_morphType == MorphType::Flip) {
			m_flipMorph.resize(dataCount);
			for (auto& [m_morphIndex, m_weight] : m_flipMorph) {
				ReadIndex(is, &m_morphIndex, m_header.m_morphIndexSize);
				Read(is, &m_weight);
			}
		} else if (m_morphType == MorphType::Impulse) {
			m_impulseMorph.resize(dataCount);
			for (auto& [m_rigidbodyIndex
				     , m_localFlag
				     , m_translateVelocity
				     , m_rotateTorque] : m_impulseMorph) {
				ReadIndex(is, &m_rigidbodyIndex, m_header.m_rigidbodyIndexSize);
				Read(is, &m_localFlag);
				Read(is, &m_translateVelocity);
				Read(is, &m_rotateTorque);
			}
		}
	}
}

void PMXReader::ReadDisplayFrame(std::istream& is) {
	int32_t displayFrameCount;
	Read(is, &displayFrameCount);
	m_displayFrames.resize(displayFrameCount);
	for (auto& [m_name, m_englishName
		     , m_flag, m_targets] : m_displayFrames) {
		ReadString(is, &m_name);
		ReadString(is, &m_englishName);
		Read(is, &m_flag);
		int32_t targetCount;
		Read(is, &targetCount);
		m_targets.resize(targetCount);
		for (auto& [m_type, m_index] : m_targets) {
			Read(is, &m_type);
			if (m_type == TargetType::BoneIndex)
				ReadIndex(is, &m_index, m_header.m_boneIndexSize);
			else if (m_type == TargetType::MorphIndex)
				ReadIndex(is, &m_index, m_header.m_morphIndexSize);
		}
	}
}

void PMXReader::ReadRigidbody(std::istream& is) {
	int32_t rbCount;
	Read(is, &rbCount);
	m_rigidBodies.resize(rbCount);
	for (auto& [m_name, m_englishName, m_boneIndex, m_group, m_collisionGroup
		     , m_shape, m_shapeSize, m_translate, m_rotate, m_mass
		     , m_translateDimmer, m_rotateDimmer
		     , m_repulsion, m_friction, m_op] : m_rigidBodies) {
		ReadString(is, &m_name);
		ReadString(is, &m_englishName);
		ReadIndex(is, &m_boneIndex, m_header.m_boneIndexSize);
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

void PMXReader::ReadJoint(std::istream& is) {
	int32_t jointCount;
	Read(is, &jointCount);
	m_joints.resize(jointCount);
	for (auto& [m_name, m_englishName, m_type, m_rigidbodyAIndex, m_rigidbodyBIndex
		     , m_translate, m_rotate, m_translateLowerLimit, m_translateUpperLimit
		     , m_rotateLowerLimit, m_rotateUpperLimit
		     , m_springTranslateFactor, m_springRotateFactor] : m_joints) {
		ReadString(is, &m_name);
		ReadString(is, &m_englishName);
		Read(is, &m_type);
		ReadIndex(is, &m_rigidbodyAIndex, m_header.m_rigidbodyIndexSize);
		ReadIndex(is, &m_rigidbodyBIndex, m_header.m_rigidbodyIndexSize);
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

void PMXReader::ReadSoftBody(std::istream& is) {
	int32_t sbCount;
	Read(is, &sbCount);
	m_softbodies.resize(sbCount);
	for (auto& [m_name, m_englishName, m_type, m_materialIndex
		     , m_group, m_collisionGroup, m_flag, m_BLinkLength
		     , m_numClusters, m_totalMass, m_collisionMargin, m_aeroModel
		     , m_VCF, m_DP, m_DG, m_LF, m_PR, m_VC, m_DF, m_MT
		     , m_CHR, m_KHR, m_SHR, m_AHR
		     , m_SRHR_CL, m_SKHR_CL, m_SSHR_CL
		     , m_SR_SPLT_CL, m_SK_SPLT_CL, m_SS_SPLT_CL
		     , m_V_IT, m_P_IT, m_D_IT, m_C_IT
		     , m_LST, m_AST, m_VST
		     , m_anchorRigidBodies, m_pinVertexIndices] : m_softbodies) {
		ReadString(is, &m_name);
		ReadString(is, &m_englishName);
		Read(is, &m_type);
		ReadIndex(is, &m_materialIndex, m_header.m_materialIndexSize);
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
			ReadIndex(is, &m_rigidBodyIndex, m_header.m_rigidbodyIndexSize);
			ReadIndex(is, &m_vertexIndex, m_header.m_vertexIndexSize);
			Read(is, &m_nearMode);
		}
		int32_t pvCount;
		Read(is, &pvCount);
		m_pinVertexIndices.resize(pvCount);
		for (auto& pv : m_pinVertexIndices)
			ReadIndex(is, &pv, m_header.m_vertexIndexSize);
	}
}

bool PMXReader::ReadPMXFile(const std::filesystem::path& filename) {
	std::ifstream is(filename, std::ios::binary);
	if (!is)
		return false;
	const auto end = GetFileEnd(is);
	ReadHeader(is);
	ReadInfo(is);
	ReadVertex(is);
	ReadFace(is);
	ReadTexture(is);
	ReadMaterial(is);
	ReadBone(is);
	ReadMorph(is);
	ReadDisplayFrame(is);
	ReadRigidbody(is);
	ReadJoint(is);
	if (HasMore(is, end))
		ReadSoftBody(is);
	return true;
}

void VMDReader::ReadHeader(std::istream& is) {
	Read(is, m_header.m_header, sizeof(m_header.m_header));
	Read(is, m_header.m_modelName, sizeof(m_header.m_modelName));
}

void VMDReader::ReadMotion(std::istream& is) {
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

void VMDReader::ReadBlendShape(std::istream& is) {
	uint32_t blendShapeCount = 0;
	Read(is, &blendShapeCount);
	m_morphs.resize(blendShapeCount);
	for (auto& [m_blendShapeName, m_frame, m_weight] : m_morphs) {
		Read(is, m_blendShapeName, sizeof(m_blendShapeName));
		Read(is, &m_frame);
		Read(is, &m_weight);
	}
}

void VMDReader::ReadCamera(std::istream& is) {
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

void VMDReader::ReadLight(std::istream& is) {
	uint32_t lightCount = 0;
	Read(is, &lightCount);
	m_lights.resize(lightCount);
	for (auto& [m_frame, m_color, m_position] : m_lights) {
		Read(is, &m_frame);
		Read(is, &m_color);
		Read(is, &m_position);
	}
}

void VMDReader::ReadShadow(std::istream& is) {
	uint32_t shadowCount = 0;
	Read(is, &shadowCount);
	m_shadows.resize(shadowCount);
	for (auto& [m_frame, m_shadowType, m_distance] : m_shadows) {
		Read(is, &m_frame);
		Read(is, &m_shadowType);
		Read(is, &m_distance);
	}
}

void VMDReader::ReadIK(std::istream& is) {
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

bool VMDReader::ReadVMDFile(const std::filesystem::path& filename) {
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
		ReadIK(is);
	return true;
}
