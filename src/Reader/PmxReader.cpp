#include "PmxReader.h"

#include "BinaryReader.h"
#include "../Util.h"

#include <fstream>

namespace Chrivent {
	void PmxReader::ReadString(std::istream& is, std::string* val) const {
		uint32_t bufSize;
		BinaryReader::Read(is, &bufSize);
		if (bufSize > 0) {
			if (header.encodeType == EncodeType::Utf16) {
				std::wstring utf16Str(bufSize / 2, L'\0');
				BinaryReader::Read(is, utf16Str.data(), bufSize);
				*val = Util::WStringToUtf8(utf16Str);
			} else if (header.encodeType == EncodeType::Utf8) {
				val->resize(bufSize);
				BinaryReader::Read(is, val->data(), bufSize);
			}
		}
	}

	void PmxReader::ReadHeader(std::istream& is) {
		BinaryReader::Read(is, header.magic, sizeof(header.magic));
		BinaryReader::Read(is, &header.version);
		BinaryReader::Read(is, &header.dataSize);
		BinaryReader::Read(is, &header.encodeType);
		BinaryReader::Read(is, &header.addUvNum);
		BinaryReader::Read(is, &header.vertexIndexSize);
		BinaryReader::Read(is, &header.textureIndexSize);
		BinaryReader::Read(is, &header.materialIndexSize);
		BinaryReader::Read(is, &header.boneIndexSize);
		BinaryReader::Read(is, &header.morphIndexSize);
		BinaryReader::Read(is, &header.rigidbodyIndexSize);
	}

	void PmxReader::ReadInfo(std::istream& is) {
		ReadString(is, &info.modelName);
		ReadString(is, &info.englishModelName);
		ReadString(is, &info.comment);
		ReadString(is, &info.englishComment);
	}

	void PmxReader::ReadVertex(std::istream& is) {
		int32_t vertexCount;
		BinaryReader::Read(is, &vertexCount);
		vertices.resize(vertexCount);
		for (auto& [position, normal, uv, addUv
				 , weightType, boneIndices, boneWeights
				 , sphericalDeformC, sphericalDeformR0, sphericalDeformR1, edgeMag] : vertices) {
			BinaryReader::Read(is, &position);
			BinaryReader::Read(is, &normal);
			BinaryReader::Read(is, &uv);
			for (uint8_t i = 0; i < header.addUvNum; i++)
				BinaryReader::Read(is, &addUv[i]);
			BinaryReader::Read(is, &weightType);
			switch (weightType) {
				case WeightType::BoneDeform1:
					BinaryReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					break;
				case WeightType::BoneDeform2:
					BinaryReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[1], header.boneIndexSize);
					BinaryReader::Read(is, &boneWeights[0]);
					break;
				case WeightType::BoneDeform4:
					BinaryReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[1], header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[2], header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[3], header.boneIndexSize);
					BinaryReader::Read(is, &boneWeights[0]);
					BinaryReader::Read(is, &boneWeights[1]);
					BinaryReader::Read(is, &boneWeights[2]);
					BinaryReader::Read(is, &boneWeights[3]);
					break;
				case WeightType::SphericalDeform:
					BinaryReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[1], header.boneIndexSize);
					BinaryReader::Read(is, &boneWeights[0]);
					BinaryReader::Read(is, &sphericalDeformC);
					BinaryReader::Read(is, &sphericalDeformR0);
					BinaryReader::Read(is, &sphericalDeformR1);
					break;
				case WeightType::QuaternionDeform:
					BinaryReader::ReadIndex(is, &boneIndices[0], header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[1], header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[2], header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[3], header.boneIndexSize);
					BinaryReader::Read(is, &boneWeights[0]);
					BinaryReader::Read(is, &boneWeights[1]);
					BinaryReader::Read(is, &boneWeights[2]);
					BinaryReader::Read(is, &boneWeights[3]);
					break;
				default: ;
			}
			BinaryReader::Read(is, &edgeMag);
				 }
	}

	void PmxReader::ReadFace(std::istream& is) {
		int32_t faceCount = 0;
		BinaryReader::Read(is, &faceCount);
		faceCount /= 3;
		faces.resize(faceCount);
		switch (header.vertexIndexSize) {
			case 1: {
				std::vector<uint8_t> faceIndices(faceCount * 3);
				BinaryReader::Read(is, faceIndices.data(), faceIndices.size());
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			case 2: {
				std::vector<uint16_t> faceIndices(faceCount * 3);
				BinaryReader::Read(is, faceIndices.data(), faceIndices.size() * sizeof(uint16_t));
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			case 4: {
				std::vector<uint32_t> faceIndices(faceCount * 3);
				BinaryReader::Read(is, faceIndices.data(), faceIndices.size() * sizeof(uint32_t));
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
		BinaryReader::Read(is, &texCount);
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
		BinaryReader::Read(is, &matCount);
		materials.resize(matCount);
		for (auto& [name, englishName, diffuse, specular
				 , specularPower, ambient, drawMode
				 , edgeColor, edgeSize
				 , textureIndex, sphereTextureIndex
				 , sphereMode, cartoonMode, cartoonTextureIndex
				 , memo, numFaceVertices] : materials) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &diffuse);
			BinaryReader::Read(is, &specular);
			BinaryReader::Read(is, &specularPower);
			BinaryReader::Read(is, &ambient);
			BinaryReader::Read(is, &drawMode);
			BinaryReader::Read(is, &edgeColor);
			BinaryReader::Read(is, &edgeSize);
			BinaryReader::ReadIndex(is, &textureIndex, header.textureIndexSize);
			BinaryReader::ReadIndex(is, &sphereTextureIndex, header.textureIndexSize);
			BinaryReader::Read(is, &sphereMode);
			BinaryReader::Read(is, &cartoonMode);
			if (cartoonMode == CartoonMode::Separate)
				BinaryReader::ReadIndex(is, &cartoonTextureIndex, header.textureIndexSize);
			else if (cartoonMode == CartoonMode::Common) {
				uint8_t cartoonIndex;
				BinaryReader::Read(is, &cartoonIndex);
				cartoonTextureIndex = static_cast<int32_t>(cartoonIndex);
			}
			ReadString(is, &memo);
			BinaryReader::Read(is, &numFaceVertices);
				 }
	}

	void PmxReader::ReadBone(std::istream& is) {
		int32_t boneCount;
		BinaryReader::Read(is, &boneCount);
		bones.resize(boneCount);
		for (auto& [name, englishName, position, parentBoneIndex
				 , deformDepth, boneFlag, positionOffset
				 , linkBoneIndex, appendBoneIndex, appendWeight
				 , fixedAxis, localXAxis, localZAxis, keyValue
				 , ikTargetBoneIndex, ikIterationCount
				 , ikLimit, ikLinks] : bones) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &position);
			BinaryReader::ReadIndex(is, &parentBoneIndex, header.boneIndexSize);
			BinaryReader::Read(is, &deformDepth);
			BinaryReader::Read(is, &boneFlag);
			if ((static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::TargetShowMode)) == 0)
				BinaryReader::Read(is, &positionOffset);
			else
				BinaryReader::ReadIndex(is, &linkBoneIndex, header.boneIndexSize);
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::AppendRotate) ||
				static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::AppendTranslate)) {
				BinaryReader::ReadIndex(is, &appendBoneIndex, header.boneIndexSize);
				BinaryReader::Read(is, &appendWeight);
				}
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::FixedAxis))
				BinaryReader::Read(is, &fixedAxis);
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::LocalAxis)) {
				BinaryReader::Read(is, &localXAxis);
				BinaryReader::Read(is, &localZAxis);
			}
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::DeformOuterParent))
				BinaryReader::Read(is, &keyValue);
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::Ik)) {
				BinaryReader::ReadIndex(is, &ikTargetBoneIndex, header.boneIndexSize);
				BinaryReader::Read(is, &ikIterationCount);
				BinaryReader::Read(is, &ikLimit);
				int32_t linkCount;
				BinaryReader::Read(is, &linkCount);
				ikLinks.resize(linkCount);
				for (auto& [ikBoneIndex
						 , enableLimit
						 , limitMin
						 , limitMax] : ikLinks) {
					BinaryReader::ReadIndex(is, &ikBoneIndex, header.boneIndexSize);
					BinaryReader::Read(is, &enableLimit);
					if (enableLimit != 0) {
						BinaryReader::Read(is, &limitMin);
						BinaryReader::Read(is, &limitMax);
					}
						 }
			}
				 }
	}

	void PmxReader::ReadMorph(std::istream& is) {
		int32_t morphCount;
		BinaryReader::Read(is, &morphCount);
		morphs.resize(morphCount);
		for (auto& [name, englishName, controlPanel, morphType
				 , positionMorph, uvMorph, boneMorph
				 , materialMorph, groupMorph
				 , flipMorph, impulseMorph] : morphs) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &controlPanel);
			BinaryReader::Read(is, &morphType);
			int32_t dataCount;
			BinaryReader::Read(is, &dataCount);
			if (morphType == MorphType::Position) {
				positionMorph.resize(dataCount);
				for (auto& [vertexIndex, position] : positionMorph) {
					BinaryReader::ReadIndex(is, &vertexIndex, header.vertexIndexSize);
					BinaryReader::Read(is, &position);
				}
			} else if (morphType == MorphType::Uv ||
					   morphType == MorphType::AddUv1 ||
					   morphType == MorphType::AddUv2 ||
					   morphType == MorphType::AddUv3 ||
					   morphType == MorphType::AddUv4) {
				uvMorph.resize(dataCount);
				for (auto& [vertexIndex, uv] : uvMorph) {
					BinaryReader::ReadIndex(is, &vertexIndex, header.vertexIndexSize);
					BinaryReader::Read(is, &uv);
				}
					   } else if (morphType == MorphType::Bone) {
					   	boneMorph.resize(dataCount);
					   	for (auto& [boneIndex, position, quaternion] : boneMorph) {
					   		BinaryReader::ReadIndex(is, &boneIndex, header.boneIndexSize);
					   		BinaryReader::Read(is, &position);
					   		BinaryReader::Read(is, &quaternion);
					   	}
					   } else if (morphType == MorphType::Material) {
					   	materialMorph.resize(dataCount);
					   	for (auto& [materialIndex, opType, diffuse
									, specular, specularPower
									, ambient, edgeColor, edgeSize
									, textureFactor, sphereTextureFactor, cartoonTextureFactor] : materialMorph) {
					   		BinaryReader::ReadIndex(is, &materialIndex, header.materialIndexSize);
					   		BinaryReader::Read(is, &opType);
					   		BinaryReader::Read(is, &diffuse);
					   		BinaryReader::Read(is, &specular);
					   		BinaryReader::Read(is, &specularPower);
					   		BinaryReader::Read(is, &ambient);
					   		BinaryReader::Read(is, &edgeColor);
					   		BinaryReader::Read(is, &edgeSize);
					   		BinaryReader::Read(is, &textureFactor);
					   		BinaryReader::Read(is, &sphereTextureFactor);
					   		BinaryReader::Read(is, &cartoonTextureFactor);
									}
					   } else if (morphType == MorphType::Group) {
					   	groupMorph.resize(dataCount);
					   	for (auto& [morphIndex, weight] : groupMorph) {
					   		BinaryReader::ReadIndex(is, &morphIndex, header.morphIndexSize);
					   		BinaryReader::Read(is, &weight);
					   	}
					   } else if (morphType == MorphType::Flip) {
					   	flipMorph.resize(dataCount);
					   	for (auto& [morphIndex, weight] : flipMorph) {
					   		BinaryReader::ReadIndex(is, &morphIndex, header.morphIndexSize);
					   		BinaryReader::Read(is, &weight);
					   	}
					   } else if (morphType == MorphType::Impulse) {
					   	impulseMorph.resize(dataCount);
					   	for (auto& [rigidbodyIndex
									, localFlag
									, translateVelocity
									, rotateTorque] : impulseMorph) {
					   		BinaryReader::ReadIndex(is, &rigidbodyIndex, header.rigidbodyIndexSize);
					   		BinaryReader::Read(is, &localFlag);
					   		BinaryReader::Read(is, &translateVelocity);
					   		BinaryReader::Read(is, &rotateTorque);
									}
					   }
				 }
	}

	void PmxReader::ReadDisplayFrame(std::istream& is) {
		int32_t displayFrameCount;
		BinaryReader::Read(is, &displayFrameCount);
		displayFrames.resize(displayFrameCount);
		for (auto& [name, englishName
				 , flag, targets] : displayFrames) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &flag);
			int32_t targetCount;
			BinaryReader::Read(is, &targetCount);
			targets.resize(targetCount);
			for (auto& [type, index] : targets) {
				BinaryReader::Read(is, &type);
				if (type == TargetType::BoneIndex)
					BinaryReader::ReadIndex(is, &index, header.boneIndexSize);
				else if (type == TargetType::MorphIndex)
					BinaryReader::ReadIndex(is, &index, header.morphIndexSize);
			}
				 }
	}

	void PmxReader::ReadRigidbody(std::istream& is) {
		int32_t rbCount;
		BinaryReader::Read(is, &rbCount);
		rigidBodies.resize(rbCount);
		for (auto& [name, englishName, boneIndex, group, collisionGroup
				 , shape, shapeSize, translate, rotate, mass
				 , translateDimmer, rotateDimmer
				 , repulsion, friction, op] : rigidBodies) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::ReadIndex(is, &boneIndex, header.boneIndexSize);
			BinaryReader::Read(is, &group);
			BinaryReader::Read(is, &collisionGroup);
			BinaryReader::Read(is, &shape);
			BinaryReader::Read(is, &shapeSize);
			BinaryReader::Read(is, &translate);
			BinaryReader::Read(is, &rotate);
			BinaryReader::Read(is, &mass);
			BinaryReader::Read(is, &translateDimmer);
			BinaryReader::Read(is, &rotateDimmer);
			BinaryReader::Read(is, &repulsion);
			BinaryReader::Read(is, &friction);
			BinaryReader::Read(is, &op);
				 }
	}

	void PmxReader::ReadJoint(std::istream& is) {
		int32_t jointCount;
		BinaryReader::Read(is, &jointCount);
		joints.resize(jointCount);
		for (auto& [name, englishName, type, rigidbodyAIndex, rigidbodyBIndex
				 , translate, rotate, translateLowerLimit, translateUpperLimit
				 , rotateLowerLimit, rotateUpperLimit
				 , springTranslateFactor, springRotateFactor] : joints) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &type);
			BinaryReader::ReadIndex(is, &rigidbodyAIndex, header.rigidbodyIndexSize);
			BinaryReader::ReadIndex(is, &rigidbodyBIndex, header.rigidbodyIndexSize);
			BinaryReader::Read(is, &translate);
			BinaryReader::Read(is, &rotate);
			BinaryReader::Read(is, &translateLowerLimit);
			BinaryReader::Read(is, &translateUpperLimit);
			BinaryReader::Read(is, &rotateLowerLimit);
			BinaryReader::Read(is, &rotateUpperLimit);
			BinaryReader::Read(is, &springTranslateFactor);
			BinaryReader::Read(is, &springRotateFactor);
				 }
	}

	void PmxReader::ReadSoftBody(std::istream& is) {
		int32_t sbCount;
		BinaryReader::Read(is, &sbCount);
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
			BinaryReader::Read(is, &type);
			BinaryReader::ReadIndex(is, &materialIndex, header.materialIndexSize);
			BinaryReader::Read(is, &group);
			BinaryReader::Read(is, &collisionGroup);
			BinaryReader::Read(is, &flag);
			BinaryReader::Read(is, &bodyLinkLength);
			BinaryReader::Read(is, &numClusters);
			BinaryReader::Read(is, &totalMass);
			BinaryReader::Read(is, &collisionMargin);
			BinaryReader::Read(is, &aeroModel);
			BinaryReader::Read(is, &vcf);
			BinaryReader::Read(is, &dp);
			BinaryReader::Read(is, &dg);
			BinaryReader::Read(is, &lf);
			BinaryReader::Read(is, &pr);
			BinaryReader::Read(is, &vc);
			BinaryReader::Read(is, &df);
			BinaryReader::Read(is, &mt);
			BinaryReader::Read(is, &chr);
			BinaryReader::Read(is, &khr);
			BinaryReader::Read(is, &shr);
			BinaryReader::Read(is, &ahr);
			BinaryReader::Read(is, &sRhrCl);
			BinaryReader::Read(is, &sKhrCl);
			BinaryReader::Read(is, &sShrCl);
			BinaryReader::Read(is, &srSplitCl);
			BinaryReader::Read(is, &skSplitCl);
			BinaryReader::Read(is, &ssSplitCl);
			BinaryReader::Read(is, &vIt);
			BinaryReader::Read(is, &pIt);
			BinaryReader::Read(is, &dIt);
			BinaryReader::Read(is, &cIt);
			BinaryReader::Read(is, &lst);
			BinaryReader::Read(is, &ast);
			BinaryReader::Read(is, &vst);
			int32_t arCount;
			BinaryReader::Read(is, &arCount);
			anchorRigidBodies.resize(arCount);
			for (auto& [rigidBodyIndex
					 , vertexIndex
					 , nearMode] : anchorRigidBodies) {
				BinaryReader::ReadIndex(is, &rigidBodyIndex, header.rigidbodyIndexSize);
				BinaryReader::ReadIndex(is, &vertexIndex, header.vertexIndexSize);
				BinaryReader::Read(is, &nearMode);
					 }
			int32_t pvCount;
			BinaryReader::Read(is, &pvCount);
			pinVertexIndices.resize(pvCount);
			for (auto& pv : pinVertexIndices)
				BinaryReader::ReadIndex(is, &pv, header.vertexIndexSize);
				 }
	}

	void PmxReader::Clear() {
		header = {};
		info = {};
		vertices.clear();
		faces.clear();
		textures.clear();
		materials.clear();
		bones.clear();
		morphs.clear();
		displayFrames.clear();
		rigidBodies.clear();
		joints.clear();
		softBodies.clear();
	}

	bool PmxReader::ReadFile(const std::filesystem::path& filename) {
		Clear();
		try {
			std::ifstream is(filename, std::ios::binary);
			if (!is)
				return false;
			const auto end = BinaryReader::GetFileEnd(is);
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
			if (BinaryReader::HasMore(is, end))
				ReadSoftBody(is);
			return true;
		} catch (...) {
			Clear();
			return false;
		}
	}
}
