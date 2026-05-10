#include "Reader.h"

#include "Util.h"

#include <fstream>

void Read(std::istream& is, void* dst, const std::size_t bytes) {
	is.read(static_cast<char*>(dst), static_cast<long long>(bytes));
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

void ReadIndex(std::istream& is, int32_t* index, const uint8_t indexSize) {
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

void PmxReader::ReadString(std::istream& is, std::string* val) const {
	uint32_t bufSize;
	Read(is, &bufSize);
	if (bufSize > 0) {
		if (header.encodeType == EncodeType::Utf16) {
			std::wstring utf16Str(bufSize / 2, L'\0');
			Read(is, utf16Str.data(), bufSize);
			*val = Util::WStringToUtf8(utf16Str);
		} else if (header.encodeType == EncodeType::Utf8) {
			val->resize(bufSize);
			Read(is, val->data(), bufSize);
		}
	}
}

void PmxReader::ReadHeader(std::istream& is) {
	Read(is, header.magic, sizeof(header.magic));
	Read(is, &header.version);
	Read(is, &header.dataSize);
	Read(is, &header.encodeType);
	Read(is, &header.addUvNum);
	Read(is, &header.vertexIndexSize);
	Read(is, &header.textureIndexSize);
	Read(is, &header.materialIndexSize);
	Read(is, &header.boneIndexSize);
	Read(is, &header.morphIndexSize);
	Read(is, &header.rigidbodyIndexSize);
}

void PmxReader::ReadInfo(std::istream& is) {
	ReadString(is, &info.modelName);
	ReadString(is, &info.englishModelName);
	ReadString(is, &info.comment);
	ReadString(is, &info.englishComment);
}

void PmxReader::ReadVertex(std::istream& is) {
	int32_t vertexCount;
	Read(is, &vertexCount);
	vertices.resize(vertexCount);
	for (auto& [position, normal, uv, addUv
		     , weightType, boneIndices, boneWeights
		     , sphericalDeformC, sphericalDeformR0, sphericalDeformR1, edgeMag] : vertices) {
		Read(is, &position);
		Read(is, &normal);
		Read(is, &uv);
		for (uint8_t i = 0; i < header.addUvNum; i++)
			Read(is, &addUv[i]);
		Read(is, &weightType);
		switch (weightType) {
			case WeightType::BoneDeform1:
				ReadIndex(is, &boneIndices[0], header.boneIndexSize);
				break;
			case WeightType::BoneDeform2:
				ReadIndex(is, &boneIndices[0], header.boneIndexSize);
				ReadIndex(is, &boneIndices[1], header.boneIndexSize);
				Read(is, &boneWeights[0]);
				break;
			case WeightType::BoneDeform4:
				ReadIndex(is, &boneIndices[0], header.boneIndexSize);
				ReadIndex(is, &boneIndices[1], header.boneIndexSize);
				ReadIndex(is, &boneIndices[2], header.boneIndexSize);
				ReadIndex(is, &boneIndices[3], header.boneIndexSize);
				Read(is, &boneWeights[0]);
				Read(is, &boneWeights[1]);
				Read(is, &boneWeights[2]);
				Read(is, &boneWeights[3]);
				break;
			case WeightType::SphericalDeform:
				ReadIndex(is, &boneIndices[0], header.boneIndexSize);
				ReadIndex(is, &boneIndices[1], header.boneIndexSize);
				Read(is, &boneWeights[0]);
				Read(is, &sphericalDeformC);
				Read(is, &sphericalDeformR0);
				Read(is, &sphericalDeformR1);
				break;
			case WeightType::QuaternionDeform:
				ReadIndex(is, &boneIndices[0], header.boneIndexSize);
				ReadIndex(is, &boneIndices[1], header.boneIndexSize);
				ReadIndex(is, &boneIndices[2], header.boneIndexSize);
				ReadIndex(is, &boneIndices[3], header.boneIndexSize);
				Read(is, &boneWeights[0]);
				Read(is, &boneWeights[1]);
				Read(is, &boneWeights[2]);
				Read(is, &boneWeights[3]);
				break;
			default: ;
		}
		Read(is, &edgeMag);
	}
}

void PmxReader::ReadFace(std::istream& is) {
	int32_t faceCount = 0;
	Read(is, &faceCount);
	faceCount /= 3;
	faces.resize(faceCount);
	switch (header.vertexIndexSize) {
		case 1: {
			std::vector<uint8_t> faceIndices(faceCount * 3);
			Read(is, faceIndices.data(), faceIndices.size());
			for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
				faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
				faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
				faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
			}
		}
		break;
		case 2: {
			std::vector<uint16_t> faceIndices(faceCount * 3);
			Read(is, faceIndices.data(), faceIndices.size() * sizeof(uint16_t));
			for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
				faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
				faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
				faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
			}
		}
		break;
		case 4: {
			std::vector<uint32_t> faceIndices(faceCount * 3);
			Read(is, faceIndices.data(), faceIndices.size() * sizeof(uint32_t));
			for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
				faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
				faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
				faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
			}
		}
		break;
		default: ;
	}
}

void PmxReader::ReadTexture(std::istream& is) {
	int32_t texCount = 0;
	Read(is, &texCount);
	textures.resize(texCount);
	std::string utf8;
	for (auto& [textureName] : textures) {
		ReadString(is, &utf8);
		const auto* p = reinterpret_cast<const char8_t*>(utf8.data());
		textureName = std::filesystem::path(std::u8string(p, p + utf8.size()));
	}
}

void PmxReader::ReadMaterial(std::istream& is) {
	int32_t matCount = 0;
	Read(is, &matCount);
	materials.resize(matCount);
	for (auto& [name, englishName, diffuse, specular
		     , specularPower, ambient, drawMode
		     , edgeColor, edgeSize
		     , textureIndex, sphereTextureIndex
		     , sphereMode, cartoonMode, cartoonTextureIndex
		     , memo, numFaceVertices] : materials) {
		ReadString(is, &name);
		ReadString(is, &englishName);
		Read(is, &diffuse);
		Read(is, &specular);
		Read(is, &specularPower);
		Read(is, &ambient);
		Read(is, &drawMode);
		Read(is, &edgeColor);
		Read(is, &edgeSize);
		ReadIndex(is, &textureIndex, header.textureIndexSize);
		ReadIndex(is, &sphereTextureIndex, header.textureIndexSize);
		Read(is, &sphereMode);
		Read(is, &cartoonMode);
		if (cartoonMode == CartoonMode::Separate)
			ReadIndex(is, &cartoonTextureIndex, header.textureIndexSize);
		else if (cartoonMode == CartoonMode::Common) {
			uint8_t cartoonIndex;
			Read(is, &cartoonIndex);
			cartoonTextureIndex = static_cast<int32_t>(cartoonIndex);
		}
		ReadString(is, &memo);
		Read(is, &numFaceVertices);
	}
}

void PmxReader::ReadBone(std::istream& is) {
	int32_t boneCount;
	Read(is, &boneCount);
	bones.resize(boneCount);
	for (auto& [name, englishName, position, parentBoneIndex
		     , deformDepth, boneFlag, positionOffset
		     , linkBoneIndex, appendBoneIndex, appendWeight
		     , fixedAxis, localXAxis, localZAxis, keyValue
		     , ikTargetBoneIndex, ikIterationCount
		     , ikLimit, ikLinks] : bones) {
		ReadString(is, &name);
		ReadString(is, &englishName);
		Read(is, &position);
		ReadIndex(is, &parentBoneIndex, header.boneIndexSize);
		Read(is, &deformDepth);
		Read(is, &boneFlag);
		if ((static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::TargetShowMode)) == 0)
			Read(is, &positionOffset);
		else
			ReadIndex(is, &linkBoneIndex, header.boneIndexSize);
		if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::AppendRotate) ||
		    static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::AppendTranslate)) {
			ReadIndex(is, &appendBoneIndex, header.boneIndexSize);
			Read(is, &appendWeight);
		}
		if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::FixedAxis))
			Read(is, &fixedAxis);
		if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::LocalAxis)) {
			Read(is, &localXAxis);
			Read(is, &localZAxis);
		}
		if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::DeformOuterParent))
			Read(is, &keyValue);
		if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::Ik)) {
			ReadIndex(is, &ikTargetBoneIndex, header.boneIndexSize);
			Read(is, &ikIterationCount);
			Read(is, &ikLimit);
			int32_t linkCount;
			Read(is, &linkCount);
			ikLinks.resize(linkCount);
			for (auto& [ikBoneIndex
				     , enableLimit
				     , limitMin
				     , limitMax] : ikLinks) {
				ReadIndex(is, &ikBoneIndex, header.boneIndexSize);
				Read(is, &enableLimit);
				if (enableLimit != 0) {
					Read(is, &limitMin);
					Read(is, &limitMax);
				}
			}
		}
	}
}

void PmxReader::ReadMorph(std::istream& is) {
	int32_t morphCount;
	Read(is, &morphCount);
	morphs.resize(morphCount);
	for (auto& [name, englishName, controlPanel, morphType
		     , positionMorph, uvMorph, boneMorph
		     , materialMorph, groupMorph
		     , flipMorph, impulseMorph] : morphs) {
		ReadString(is, &name);
		ReadString(is, &englishName);
		Read(is, &controlPanel);
		Read(is, &morphType);
		int32_t dataCount;
		Read(is, &dataCount);
		if (morphType == MorphType::Position) {
			positionMorph.resize(dataCount);
			for (auto& [vertexIndex, position] : positionMorph) {
				ReadIndex(is, &vertexIndex, header.vertexIndexSize);
				Read(is, &position);
			}
		} else if (morphType == MorphType::Uv ||
		           morphType == MorphType::AddUv1 ||
		           morphType == MorphType::AddUv2 ||
		           morphType == MorphType::AddUv3 ||
		           morphType == MorphType::AddUv4) {
			uvMorph.resize(dataCount);
			for (auto& [vertexIndex, uv] : uvMorph) {
				ReadIndex(is, &vertexIndex, header.vertexIndexSize);
				Read(is, &uv);
			}
		} else if (morphType == MorphType::Bone) {
			boneMorph.resize(dataCount);
			for (auto& [boneIndex, position, quaternion] : boneMorph) {
				ReadIndex(is, &boneIndex, header.boneIndexSize);
				Read(is, &position);
				Read(is, &quaternion);
			}
		} else if (morphType == MorphType::Material) {
			materialMorph.resize(dataCount);
			for (auto& [materialIndex, opType, diffuse
				     , specular, specularPower
				     , ambient, edgeColor, edgeSize
				     , textureFactor, sphereTextureFactor, cartoonTextureFactor] : materialMorph) {
				ReadIndex(is, &materialIndex, header.materialIndexSize);
				Read(is, &opType);
				Read(is, &diffuse);
				Read(is, &specular);
				Read(is, &specularPower);
				Read(is, &ambient);
				Read(is, &edgeColor);
				Read(is, &edgeSize);
				Read(is, &textureFactor);
				Read(is, &sphereTextureFactor);
				Read(is, &cartoonTextureFactor);
			}
		} else if (morphType == MorphType::Group) {
			groupMorph.resize(dataCount);
			for (auto& [morphIndex, weight] : groupMorph) {
				ReadIndex(is, &morphIndex, header.morphIndexSize);
				Read(is, &weight);
			}
		} else if (morphType == MorphType::Flip) {
			flipMorph.resize(dataCount);
			for (auto& [morphIndex, weight] : flipMorph) {
				ReadIndex(is, &morphIndex, header.morphIndexSize);
				Read(is, &weight);
			}
		} else if (morphType == MorphType::Impulse) {
			impulseMorph.resize(dataCount);
			for (auto& [rigidbodyIndex
				     , localFlag
				     , translateVelocity
				     , rotateTorque] : impulseMorph) {
				ReadIndex(is, &rigidbodyIndex, header.rigidbodyIndexSize);
				Read(is, &localFlag);
				Read(is, &translateVelocity);
				Read(is, &rotateTorque);
			}
		}
	}
}

void PmxReader::ReadDisplayFrame(std::istream& is) {
	int32_t displayFrameCount;
	Read(is, &displayFrameCount);
	displayFrames.resize(displayFrameCount);
	for (auto& [name, englishName
		     , flag, targets] : displayFrames) {
		ReadString(is, &name);
		ReadString(is, &englishName);
		Read(is, &flag);
		int32_t targetCount;
		Read(is, &targetCount);
		targets.resize(targetCount);
		for (auto& [type, index] : targets) {
			Read(is, &type);
			if (type == TargetType::BoneIndex)
				ReadIndex(is, &index, header.boneIndexSize);
			else if (type == TargetType::MorphIndex)
				ReadIndex(is, &index, header.morphIndexSize);
		}
	}
}

void PmxReader::ReadRigidbody(std::istream& is) {
	int32_t rbCount;
	Read(is, &rbCount);
	rigidBodies.resize(rbCount);
	for (auto& [name, englishName, boneIndex, group, collisionGroup
		     , shape, shapeSize, translate, rotate, mass
		     , translateDimmer, rotateDimmer
		     , repulsion, friction, op] : rigidBodies) {
		ReadString(is, &name);
		ReadString(is, &englishName);
		ReadIndex(is, &boneIndex, header.boneIndexSize);
		Read(is, &group);
		Read(is, &collisionGroup);
		Read(is, &shape);
		Read(is, &shapeSize);
		Read(is, &translate);
		Read(is, &rotate);
		Read(is, &mass);
		Read(is, &translateDimmer);
		Read(is, &rotateDimmer);
		Read(is, &repulsion);
		Read(is, &friction);
		Read(is, &op);
	}
}

void PmxReader::ReadJoint(std::istream& is) {
	int32_t jointCount;
	Read(is, &jointCount);
	joints.resize(jointCount);
	for (auto& [name, englishName, type, rigidbodyAIndex, rigidbodyBIndex
		     , translate, rotate, translateLowerLimit, translateUpperLimit
		     , rotateLowerLimit, rotateUpperLimit
		     , springTranslateFactor, springRotateFactor] : joints) {
		ReadString(is, &name);
		ReadString(is, &englishName);
		Read(is, &type);
		ReadIndex(is, &rigidbodyAIndex, header.rigidbodyIndexSize);
		ReadIndex(is, &rigidbodyBIndex, header.rigidbodyIndexSize);
		Read(is, &translate);
		Read(is, &rotate);
		Read(is, &translateLowerLimit);
		Read(is, &translateUpperLimit);
		Read(is, &rotateLowerLimit);
		Read(is, &rotateUpperLimit);
		Read(is, &springTranslateFactor);
		Read(is, &springRotateFactor);
	}
}

void PmxReader::ReadSoftBody(std::istream& is) {
	int32_t sbCount;
	Read(is, &sbCount);
	softBodies.resize(sbCount);
	for (auto& [name, englishName, type, materialIndex
		     , group, collisionGroup, flag, bodyLinkLength
		     , numClusters, totalMass, collisionMargin, aeroModel
		     , vcf, dp, dg, lf, pr, vc, df, mt
		     , chr, khr, shr, ahr
		     , sRhrCl, sKhrCl, sShrCl
		     , srSplitCl, skSplitCl, ssSplitCl
		     , vIt, pIt, dIt, cIt
		     , lst, ast, vst
		     , anchorRigidBodies, pinVertexIndices] : softBodies) {
		ReadString(is, &name);
		ReadString(is, &englishName);
		Read(is, &type);
		ReadIndex(is, &materialIndex, header.materialIndexSize);
		Read(is, &group);
		Read(is, &collisionGroup);
		Read(is, &flag);
		Read(is, &bodyLinkLength);
		Read(is, &numClusters);
		Read(is, &totalMass);
		Read(is, &collisionMargin);
		Read(is, &aeroModel);
		Read(is, &vcf);
		Read(is, &dp);
		Read(is, &dg);
		Read(is, &lf);
		Read(is, &pr);
		Read(is, &vc);
		Read(is, &df);
		Read(is, &mt);
		Read(is, &chr);
		Read(is, &khr);
		Read(is, &shr);
		Read(is, &ahr);
		Read(is, &sRhrCl);
		Read(is, &sKhrCl);
		Read(is, &sShrCl);
		Read(is, &srSplitCl);
		Read(is, &skSplitCl);
		Read(is, &ssSplitCl);
		Read(is, &vIt);
		Read(is, &pIt);
		Read(is, &dIt);
		Read(is, &cIt);
		Read(is, &lst);
		Read(is, &ast);
		Read(is, &vst);
		int32_t arCount;
		Read(is, &arCount);
		anchorRigidBodies.resize(arCount);
		for (auto& [rigidBodyIndex
			     , vertexIndex
			     , nearMode] : anchorRigidBodies) {
			ReadIndex(is, &rigidBodyIndex, header.rigidbodyIndexSize);
			ReadIndex(is, &vertexIndex, header.vertexIndexSize);
			Read(is, &nearMode);
		}
		int32_t pvCount;
		Read(is, &pvCount);
		pinVertexIndices.resize(pvCount);
		for (auto& pv : pinVertexIndices)
			ReadIndex(is, &pv, header.vertexIndexSize);
	}
}

bool PmxReader::ReadFile(const std::filesystem::path& filename) {
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
