#include "PmxReader.h"

#include "BinaryStreamReader.h"
#include "../Util.h"

#include <fstream>

namespace Chrivent {
	void PmxReader::ReadString(std::istream& is, std::string* val) const {
		uint32_t bufSize;
		BinaryStreamReader::Read(is, &bufSize);
		if (bufSize > 0) {
			if (header.encodeType == EncodeType::Utf16) {
				std::wstring utf16Str(bufSize / 2, L'\0');
				BinaryStreamReader::Read(is, utf16Str.data(), bufSize);
				*val = Util::WStringToUtf8(utf16Str);
			} else if (header.encodeType == EncodeType::Utf8) {
				val->resize(bufSize);
				BinaryStreamReader::Read(is, val->data(), bufSize);
			}
		}
	}

	void PmxReader::ReadHeader(std::istream& is) {
		BinaryStreamReader::Read(is, header.magic, sizeof(header.magic));
		BinaryStreamReader::Read(is, &header.version);
		BinaryStreamReader::Read(is, &header.dataSize);
		BinaryStreamReader::Read(is, &header.encodeType);
		BinaryStreamReader::Read(is, &header.addUvNum);
		BinaryStreamReader::Read(is, &header.vertexIndexSize);
		BinaryStreamReader::Read(is, &header.textureIndexSize);
		BinaryStreamReader::Read(is, &header.materialIndexSize);
		BinaryStreamReader::Read(is, &header.boneIndexSize);
		BinaryStreamReader::Read(is, &header.morphIndexSize);
		BinaryStreamReader::Read(is, &header.rigidbodyIndexSize);
	}

	void PmxReader::ReadInfo(std::istream& is) {
		ReadString(is, &info.modelName);
		ReadString(is, &info.englishModelName);
		ReadString(is, &info.comment);
		ReadString(is, &info.englishComment);
	}

	void PmxReader::ReadVertex(std::istream& is) {
		int32_t vertexCount;
		BinaryStreamReader::Read(is, &vertexCount);
		vertices.resize(vertexCount);
		for (auto& [position, normal, uv, addUv
				 , weightType, boneIndices, boneWeights
				 , sphericalDeformC, sphericalDeformR0, sphericalDeformR1, edgeMag] : vertices) {
			BinaryStreamReader::Read(is, &position);
			BinaryStreamReader::Read(is, &normal);
			BinaryStreamReader::Read(is, &uv);
			for (uint8_t i = 0; i < header.addUvNum; i++)
				BinaryStreamReader::Read(is, &addUv[i]);
			BinaryStreamReader::Read(is, &weightType);
			switch (weightType) {
				case WeightType::BoneDeform1:
					BinaryStreamReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					break;
				case WeightType::BoneDeform2:
					BinaryStreamReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					BinaryStreamReader::ReadIndex(is, &boneIndices[1], header.boneIndexSize);
					BinaryStreamReader::Read(is, &boneWeights[0]);
					break;
				case WeightType::BoneDeform4:
					BinaryStreamReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					BinaryStreamReader::ReadIndex(is, &boneIndices[1], header.boneIndexSize);
					BinaryStreamReader::ReadIndex(is, &boneIndices[2], header.boneIndexSize);
					BinaryStreamReader::ReadIndex(is, &boneIndices[3], header.boneIndexSize);
					BinaryStreamReader::Read(is, &boneWeights[0]);
					BinaryStreamReader::Read(is, &boneWeights[1]);
					BinaryStreamReader::Read(is, &boneWeights[2]);
					BinaryStreamReader::Read(is, &boneWeights[3]);
					break;
				case WeightType::SphericalDeform:
					BinaryStreamReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					BinaryStreamReader::ReadIndex(is, &boneIndices[1], header.boneIndexSize);
					BinaryStreamReader::Read(is, &boneWeights[0]);
					BinaryStreamReader::Read(is, &sphericalDeformC);
					BinaryStreamReader::Read(is, &sphericalDeformR0);
					BinaryStreamReader::Read(is, &sphericalDeformR1);
					break;
				case WeightType::QuaternionDeform:
					BinaryStreamReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					BinaryStreamReader::ReadIndex(is, &boneIndices[1], header.boneIndexSize);
					BinaryStreamReader::ReadIndex(is, &boneIndices[2], header.boneIndexSize);
					BinaryStreamReader::ReadIndex(is, &boneIndices[3], header.boneIndexSize);
					BinaryStreamReader::Read(is, &boneWeights[0]);
					BinaryStreamReader::Read(is, &boneWeights[1]);
					BinaryStreamReader::Read(is, &boneWeights[2]);
					BinaryStreamReader::Read(is, &boneWeights[3]);
					break;
				default: ;
			}
			BinaryStreamReader::Read(is, &edgeMag);
				 }
	}

	void PmxReader::ReadFace(std::istream& is) {
		int32_t faceCount = 0;
		BinaryStreamReader::Read(is, &faceCount);
		faceCount /= 3;
		faces.resize(faceCount);
		switch (header.vertexIndexSize) {
			case 1: {
				std::vector<uint8_t> faceIndices(faceCount * 3);
				BinaryStreamReader::Read(is, faceIndices.data(), faceIndices.size());
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			case 2: {
				std::vector<uint16_t> faceIndices(faceCount * 3);
				BinaryStreamReader::Read(is, faceIndices.data(), faceIndices.size() * sizeof(uint16_t));
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			case 4: {
				std::vector<uint32_t> faceIndices(faceCount * 3);
				BinaryStreamReader::Read(is, faceIndices.data(), faceIndices.size() * sizeof(uint32_t));
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
		BinaryStreamReader::Read(is, &texCount);
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
		BinaryStreamReader::Read(is, &matCount);
		materials.resize(matCount);
		for (auto& [name, englishName, diffuse, specular
				 , specularPower, ambient, drawMode
				 , edgeColor, edgeSize
				 , textureIndex, sphereTextureIndex
				 , sphereMode, cartoonMode, cartoonTextureIndex
				 , memo, numFaceVertices] : materials) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryStreamReader::Read(is, &diffuse);
			BinaryStreamReader::Read(is, &specular);
			BinaryStreamReader::Read(is, &specularPower);
			BinaryStreamReader::Read(is, &ambient);
			BinaryStreamReader::Read(is, &drawMode);
			BinaryStreamReader::Read(is, &edgeColor);
			BinaryStreamReader::Read(is, &edgeSize);
			BinaryStreamReader::ReadIndex(is, &textureIndex, header.textureIndexSize);
			BinaryStreamReader::ReadIndex(is, &sphereTextureIndex, header.textureIndexSize);
			BinaryStreamReader::Read(is, &sphereMode);
			BinaryStreamReader::Read(is, &cartoonMode);
			if (cartoonMode == CartoonMode::Separate)
				BinaryStreamReader::ReadIndex(is, &cartoonTextureIndex, header.textureIndexSize);
			else if (cartoonMode == CartoonMode::Common) {
				uint8_t cartoonIndex;
				BinaryStreamReader::Read(is, &cartoonIndex);
				cartoonTextureIndex = static_cast<int32_t>(cartoonIndex);
			}
			ReadString(is, &memo);
			BinaryStreamReader::Read(is, &numFaceVertices);
				 }
	}

	void PmxReader::ReadBone(std::istream& is) {
		int32_t boneCount;
		BinaryStreamReader::Read(is, &boneCount);
		bones.resize(boneCount);
		for (auto& [name, englishName, position, parentBoneIndex
				 , deformDepth, boneFlag, positionOffset
				 , linkBoneIndex, appendBoneIndex, appendWeight
				 , fixedAxis, localXAxis, localZAxis, keyValue
				 , ikTargetBoneIndex, ikIterationCount
				 , ikLimit, ikLinks] : bones) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryStreamReader::Read(is, &position);
			BinaryStreamReader::ReadIndex(is, &parentBoneIndex, header.boneIndexSize);
			BinaryStreamReader::Read(is, &deformDepth);
			BinaryStreamReader::Read(is, &boneFlag);
			if ((static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::TargetShowMode)) == 0)
				BinaryStreamReader::Read(is, &positionOffset);
			else
				BinaryStreamReader::ReadIndex(is, &linkBoneIndex, header.boneIndexSize);
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::AppendRotate) ||
				static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::AppendTranslate)) {
				BinaryStreamReader::ReadIndex(is, &appendBoneIndex, header.boneIndexSize);
				BinaryStreamReader::Read(is, &appendWeight);
				}
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::FixedAxis))
				BinaryStreamReader::Read(is, &fixedAxis);
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::LocalAxis)) {
				BinaryStreamReader::Read(is, &localXAxis);
				BinaryStreamReader::Read(is, &localZAxis);
			}
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::DeformOuterParent))
				BinaryStreamReader::Read(is, &keyValue);
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::Ik)) {
				BinaryStreamReader::ReadIndex(is, &ikTargetBoneIndex, header.boneIndexSize);
				BinaryStreamReader::Read(is, &ikIterationCount);
				BinaryStreamReader::Read(is, &ikLimit);
				int32_t linkCount;
				BinaryStreamReader::Read(is, &linkCount);
				ikLinks.resize(linkCount);
				for (auto& [ikBoneIndex
						 , enableLimit
						 , limitMin
						 , limitMax] : ikLinks) {
					BinaryStreamReader::ReadIndex(is, &ikBoneIndex, header.boneIndexSize);
					BinaryStreamReader::Read(is, &enableLimit);
					if (enableLimit != 0) {
						BinaryStreamReader::Read(is, &limitMin);
						BinaryStreamReader::Read(is, &limitMax);
					}
						 }
			}
				 }
	}

	void PmxReader::ReadMorph(std::istream& is) {
		int32_t morphCount;
		BinaryStreamReader::Read(is, &morphCount);
		morphs.resize(morphCount);
		for (auto& [name, englishName, controlPanel, morphType
				 , positionMorph, uvMorph, boneMorph
				 , materialMorph, groupMorph
				 , flipMorph, impulseMorph] : morphs) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryStreamReader::Read(is, &controlPanel);
			BinaryStreamReader::Read(is, &morphType);
			int32_t dataCount;
			BinaryStreamReader::Read(is, &dataCount);
			if (morphType == MorphType::Position) {
				positionMorph.resize(dataCount);
				for (auto& [vertexIndex, position] : positionMorph) {
					BinaryStreamReader::ReadIndex(is, &vertexIndex, header.vertexIndexSize);
					BinaryStreamReader::Read(is, &position);
				}
			} else if (morphType == MorphType::Uv ||
					   morphType == MorphType::AddUv1 ||
					   morphType == MorphType::AddUv2 ||
					   morphType == MorphType::AddUv3 ||
					   morphType == MorphType::AddUv4) {
				uvMorph.resize(dataCount);
				for (auto& [vertexIndex, uv] : uvMorph) {
					BinaryStreamReader::ReadIndex(is, &vertexIndex, header.vertexIndexSize);
					BinaryStreamReader::Read(is, &uv);
				}
					   } else if (morphType == MorphType::Bone) {
					   	boneMorph.resize(dataCount);
					   	for (auto& [boneIndex, position, quaternion] : boneMorph) {
					   		BinaryStreamReader::ReadIndex(is, &boneIndex, header.boneIndexSize);
					   		BinaryStreamReader::Read(is, &position);
					   		BinaryStreamReader::Read(is, &quaternion);
					   	}
					   } else if (morphType == MorphType::Material) {
					   	materialMorph.resize(dataCount);
					   	for (auto& [materialIndex, opType, diffuse
									, specular, specularPower
									, ambient, edgeColor, edgeSize
									, textureFactor, sphereTextureFactor, cartoonTextureFactor] : materialMorph) {
					   		BinaryStreamReader::ReadIndex(is, &materialIndex, header.materialIndexSize);
					   		BinaryStreamReader::Read(is, &opType);
					   		BinaryStreamReader::Read(is, &diffuse);
					   		BinaryStreamReader::Read(is, &specular);
					   		BinaryStreamReader::Read(is, &specularPower);
					   		BinaryStreamReader::Read(is, &ambient);
					   		BinaryStreamReader::Read(is, &edgeColor);
					   		BinaryStreamReader::Read(is, &edgeSize);
					   		BinaryStreamReader::Read(is, &textureFactor);
					   		BinaryStreamReader::Read(is, &sphereTextureFactor);
					   		BinaryStreamReader::Read(is, &cartoonTextureFactor);
									}
					   } else if (morphType == MorphType::Group) {
					   	groupMorph.resize(dataCount);
					   	for (auto& [morphIndex, weight] : groupMorph) {
					   		BinaryStreamReader::ReadIndex(is, &morphIndex, header.morphIndexSize);
					   		BinaryStreamReader::Read(is, &weight);
					   	}
					   } else if (morphType == MorphType::Flip) {
					   	flipMorph.resize(dataCount);
					   	for (auto& [morphIndex, weight] : flipMorph) {
					   		BinaryStreamReader::ReadIndex(is, &morphIndex, header.morphIndexSize);
					   		BinaryStreamReader::Read(is, &weight);
					   	}
					   } else if (morphType == MorphType::Impulse) {
					   	impulseMorph.resize(dataCount);
					   	for (auto& [rigidbodyIndex
									, localFlag
									, translateVelocity
									, rotateTorque] : impulseMorph) {
					   		BinaryStreamReader::ReadIndex(is, &rigidbodyIndex, header.rigidbodyIndexSize);
					   		BinaryStreamReader::Read(is, &localFlag);
					   		BinaryStreamReader::Read(is, &translateVelocity);
					   		BinaryStreamReader::Read(is, &rotateTorque);
									}
					   }
				 }
	}

	void PmxReader::ReadDisplayFrame(std::istream& is) {
		int32_t displayFrameCount;
		BinaryStreamReader::Read(is, &displayFrameCount);
		displayFrames.resize(displayFrameCount);
		for (auto& [name, englishName
				 , flag, targets] : displayFrames) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryStreamReader::Read(is, &flag);
			int32_t targetCount;
			BinaryStreamReader::Read(is, &targetCount);
			targets.resize(targetCount);
			for (auto& [type, index] : targets) {
				BinaryStreamReader::Read(is, &type);
				if (type == TargetType::BoneIndex)
					BinaryStreamReader::ReadIndex(is, &index, header.boneIndexSize);
				else if (type == TargetType::MorphIndex)
					BinaryStreamReader::ReadIndex(is, &index, header.morphIndexSize);
			}
				 }
	}

	void PmxReader::ReadRigidbody(std::istream& is) {
		int32_t rbCount;
		BinaryStreamReader::Read(is, &rbCount);
		rigidBodies.resize(rbCount);
		for (auto& [name, englishName, boneIndex, group, collisionGroup
				 , shape, shapeSize, translate, rotate, mass
				 , translateDimmer, rotateDimmer
				 , repulsion, friction, op] : rigidBodies) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryStreamReader::ReadIndex(is, &boneIndex, header.boneIndexSize);
			BinaryStreamReader::Read(is, &group);
			BinaryStreamReader::Read(is, &collisionGroup);
			BinaryStreamReader::Read(is, &shape);
			BinaryStreamReader::Read(is, &shapeSize);
			BinaryStreamReader::Read(is, &translate);
			BinaryStreamReader::Read(is, &rotate);
			BinaryStreamReader::Read(is, &mass);
			BinaryStreamReader::Read(is, &translateDimmer);
			BinaryStreamReader::Read(is, &rotateDimmer);
			BinaryStreamReader::Read(is, &repulsion);
			BinaryStreamReader::Read(is, &friction);
			BinaryStreamReader::Read(is, &op);
				 }
	}

	void PmxReader::ReadJoint(std::istream& is) {
		int32_t jointCount;
		BinaryStreamReader::Read(is, &jointCount);
		joints.resize(jointCount);
		for (auto& [name, englishName, type, rigidbodyAIndex, rigidbodyBIndex
				 , translate, rotate, translateLowerLimit, translateUpperLimit
				 , rotateLowerLimit, rotateUpperLimit
				 , springTranslateFactor, springRotateFactor] : joints) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryStreamReader::Read(is, &type);
			BinaryStreamReader::ReadIndex(is, &rigidbodyAIndex, header.rigidbodyIndexSize);
			BinaryStreamReader::ReadIndex(is, &rigidbodyBIndex, header.rigidbodyIndexSize);
			BinaryStreamReader::Read(is, &translate);
			BinaryStreamReader::Read(is, &rotate);
			BinaryStreamReader::Read(is, &translateLowerLimit);
			BinaryStreamReader::Read(is, &translateUpperLimit);
			BinaryStreamReader::Read(is, &rotateLowerLimit);
			BinaryStreamReader::Read(is, &rotateUpperLimit);
			BinaryStreamReader::Read(is, &springTranslateFactor);
			BinaryStreamReader::Read(is, &springRotateFactor);
				 }
	}

	void PmxReader::ReadSoftBody(std::istream& is) {
		int32_t sbCount;
		BinaryStreamReader::Read(is, &sbCount);
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
			BinaryStreamReader::Read(is, &type);
			BinaryStreamReader::ReadIndex(is, &materialIndex, header.materialIndexSize);
			BinaryStreamReader::Read(is, &group);
			BinaryStreamReader::Read(is, &collisionGroup);
			BinaryStreamReader::Read(is, &flag);
			BinaryStreamReader::Read(is, &bodyLinkLength);
			BinaryStreamReader::Read(is, &numClusters);
			BinaryStreamReader::Read(is, &totalMass);
			BinaryStreamReader::Read(is, &collisionMargin);
			BinaryStreamReader::Read(is, &aeroModel);
			BinaryStreamReader::Read(is, &vcf);
			BinaryStreamReader::Read(is, &dp);
			BinaryStreamReader::Read(is, &dg);
			BinaryStreamReader::Read(is, &lf);
			BinaryStreamReader::Read(is, &pr);
			BinaryStreamReader::Read(is, &vc);
			BinaryStreamReader::Read(is, &df);
			BinaryStreamReader::Read(is, &mt);
			BinaryStreamReader::Read(is, &chr);
			BinaryStreamReader::Read(is, &khr);
			BinaryStreamReader::Read(is, &shr);
			BinaryStreamReader::Read(is, &ahr);
			BinaryStreamReader::Read(is, &sRhrCl);
			BinaryStreamReader::Read(is, &sKhrCl);
			BinaryStreamReader::Read(is, &sShrCl);
			BinaryStreamReader::Read(is, &srSplitCl);
			BinaryStreamReader::Read(is, &skSplitCl);
			BinaryStreamReader::Read(is, &ssSplitCl);
			BinaryStreamReader::Read(is, &vIt);
			BinaryStreamReader::Read(is, &pIt);
			BinaryStreamReader::Read(is, &dIt);
			BinaryStreamReader::Read(is, &cIt);
			BinaryStreamReader::Read(is, &lst);
			BinaryStreamReader::Read(is, &ast);
			BinaryStreamReader::Read(is, &vst);
			int32_t arCount;
			BinaryStreamReader::Read(is, &arCount);
			anchorRigidBodies.resize(arCount);
			for (auto& [rigidBodyIndex
					 , vertexIndex
					 , nearMode] : anchorRigidBodies) {
				BinaryStreamReader::ReadIndex(is, &rigidBodyIndex, header.rigidbodyIndexSize);
				BinaryStreamReader::ReadIndex(is, &vertexIndex, header.vertexIndexSize);
				BinaryStreamReader::Read(is, &nearMode);
					 }
			int32_t pvCount;
			BinaryStreamReader::Read(is, &pvCount);
			pinVertexIndices.resize(pvCount);
			for (auto& pv : pinVertexIndices)
				BinaryStreamReader::ReadIndex(is, &pv, header.vertexIndexSize);
				 }
	}

	bool PmxReader::ReadFile(const std::filesystem::path& filename) {
		std::ifstream is(filename, std::ios::binary);
		if (!is)
			return false;
		const auto end = BinaryStreamReader::GetFileEnd(is);
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
		if (BinaryStreamReader::HasMore(is, end))
			ReadSoftBody(is);
		return true;
	}
}
