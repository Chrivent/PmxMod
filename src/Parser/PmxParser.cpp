#include "PmxParser.h"

#include "BinaryReader.h"
#include "../Util.h"

#include <fstream>
#include <iostream>

namespace Chrivent {
	void PmxParser::ReadString(std::istream& is, std::string* val) const {
		uint32_t bufSize;
		BinaryReader::Read(is, &bufSize);
		if (bufSize > 0) {
			if (data.header.encodeType == EncodeType::Utf16) {
				std::wstring utf16Str(bufSize / 2, L'\0');
				BinaryReader::Read(is, utf16Str.data(), bufSize);
				*val = Util::WStringToUtf8(utf16Str);
			} else if (data.header.encodeType == EncodeType::Utf8) {
				val->resize(bufSize);
				BinaryReader::Read(is, val->data(), bufSize);
			}
		}
	}

	void PmxParser::ReadHeader(std::istream& is) {
		BinaryReader::Read(is, data.header.magic, sizeof(data.header.magic));
		BinaryReader::Read(is, &data.header.version);
		BinaryReader::Read(is, &data.header.dataSize);
		BinaryReader::Read(is, &data.header.encodeType);
		BinaryReader::Read(is, &data.header.addUvNum);
		BinaryReader::Read(is, &data.header.vertexIndexSize);
		BinaryReader::Read(is, &data.header.textureIndexSize);
		BinaryReader::Read(is, &data.header.materialIndexSize);
		BinaryReader::Read(is, &data.header.boneIndexSize);
		BinaryReader::Read(is, &data.header.morphIndexSize);
		BinaryReader::Read(is, &data.header.rigidbodyIndexSize);
	}

	void PmxParser::ReadInfo(std::istream& is) {
		ReadString(is, &data.info.modelName);
		ReadString(is, &data.info.englishModelName);
		ReadString(is, &data.info.comment);
		ReadString(is, &data.info.englishComment);
	}

	void PmxParser::ReadVertex(std::istream& is) {
		int32_t vertexCount;
		BinaryReader::Read(is, &vertexCount);
		data.vertices.resize(vertexCount);
		for (auto& [position, normal, uv, addUv
				 , weightType, boneIndices, boneWeights
				 , sphericalDeformC, sphericalDeformR0, sphericalDeformR1, edgeMag] : data.vertices) {
			BinaryReader::Read(is, &position);
			BinaryReader::Read(is, &normal);
			BinaryReader::Read(is, &uv);
			for (uint8_t i = 0; i < data.header.addUvNum; i++)
				BinaryReader::Read(is, &addUv[i]);
			BinaryReader::Read(is, &weightType);
			switch (weightType) {
				case WeightType::BoneDeform1:
					BinaryReader::ReadIndex(is, &boneIndices[0], data.header.boneIndexSize);
					break;
				case WeightType::BoneDeform2:
					BinaryReader::ReadIndex(is, &boneIndices[0], data.header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[1], data.header.boneIndexSize);
					BinaryReader::Read(is, &boneWeights[0]);
					break;
				case WeightType::BoneDeform4:
					BinaryReader::ReadIndex(is, &boneIndices[0], data.header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[1], data.header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[2], data.header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[3], data.header.boneIndexSize);
					BinaryReader::Read(is, &boneWeights[0]);
					BinaryReader::Read(is, &boneWeights[1]);
					BinaryReader::Read(is, &boneWeights[2]);
					BinaryReader::Read(is, &boneWeights[3]);
					break;
				case WeightType::SphericalDeform:
					BinaryReader::ReadIndex(is, &boneIndices[0], data.header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[1], data.header.boneIndexSize);
					BinaryReader::Read(is, &boneWeights[0]);
					BinaryReader::Read(is, &sphericalDeformC);
					BinaryReader::Read(is, &sphericalDeformR0);
					BinaryReader::Read(is, &sphericalDeformR1);
					break;
				case WeightType::QuaternionDeform:
					BinaryReader::ReadIndex(is, &boneIndices[0], data.header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[1], data.header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[2], data.header.boneIndexSize);
					BinaryReader::ReadIndex(is, &boneIndices[3], data.header.boneIndexSize);
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

	void PmxParser::ReadFace(std::istream& is) {
		int32_t faceCount = 0;
		BinaryReader::Read(is, &faceCount);
		faceCount /= 3;
		data.faces.resize(faceCount);
		switch (data.header.vertexIndexSize) {
			case 1: {
				std::vector<uint8_t> faceIndices(faceCount * 3);
				BinaryReader::Read(is, faceIndices.data(), faceIndices.size());
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					data.faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					data.faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					data.faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			case 2: {
				std::vector<uint16_t> faceIndices(faceCount * 3);
				BinaryReader::Read(is, faceIndices.data(), faceIndices.size() * sizeof(uint16_t));
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					data.faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					data.faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					data.faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			case 4: {
				std::vector<uint32_t> faceIndices(faceCount * 3);
				BinaryReader::Read(is, faceIndices.data(), faceIndices.size() * sizeof(uint32_t));
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					data.faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					data.faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					data.faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			default: ;
		}
	}

	void PmxParser::ReadTexture(std::istream& is) {
		int32_t texCount = 0;
		BinaryReader::Read(is, &texCount);
		data.textures.resize(texCount);
		std::string utf8;
		for (auto& [textureName] : data.textures) {
			ReadString(is, &utf8);
			const auto* p = reinterpret_cast<const char8_t*>(utf8.data());
			textureName = std::filesystem::path(std::u8string(p, p + utf8.size()));
		}
	}

	void PmxParser::ReadMaterial(std::istream& is) {
		int32_t matCount = 0;
		BinaryReader::Read(is, &matCount);
		data.materials.resize(matCount);
		for (auto& [name, englishName, diffuse, specular
				 , specularPower, ambient, drawMode
				 , edgeColor, edgeSize
				 , textureIndex, sphereTextureIndex
				 , sphereMode, toonMode, toonTextureIndex
				 , memo, numFaceVertices] : data.materials) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &diffuse);
			BinaryReader::Read(is, &specular);
			BinaryReader::Read(is, &specularPower);
			BinaryReader::Read(is, &ambient);
			BinaryReader::Read(is, &drawMode);
			BinaryReader::Read(is, &edgeColor);
			BinaryReader::Read(is, &edgeSize);
			BinaryReader::ReadIndex(is, &textureIndex, data.header.textureIndexSize);
			BinaryReader::ReadIndex(is, &sphereTextureIndex, data.header.textureIndexSize);
			BinaryReader::Read(is, &sphereMode);
			BinaryReader::Read(is, &toonMode);
			if (toonMode == ToonMode::Separate)
				BinaryReader::ReadIndex(is, &toonTextureIndex, data.header.textureIndexSize);
			else if (toonMode == ToonMode::Common) {
				uint8_t toonIndex;
				BinaryReader::Read(is, &toonIndex);
				toonTextureIndex = toonIndex;
			}
			ReadString(is, &memo);
			BinaryReader::Read(is, &numFaceVertices);
				 }
	}

	void PmxParser::ReadBone(std::istream& is) {
		int32_t boneCount;
		BinaryReader::Read(is, &boneCount);
		data.bones.resize(boneCount);
		for (auto& [name, englishName, position, parentBoneIndex
				 , deformDepth, boneFlag, positionOffset
				 , linkBoneIndex, appendBoneIndex, appendWeight
				 , fixedAxis, localXAxis, localZAxis, keyValue
				 , ikTargetBoneIndex, ikIterationCount
				 , ikLimit, ikLinks] : data.bones) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &position);
			BinaryReader::ReadIndex(is, &parentBoneIndex, data.header.boneIndexSize);
			BinaryReader::Read(is, &deformDepth);
			BinaryReader::Read(is, &boneFlag);
			if ((static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::TargetShowMode)) == 0)
				BinaryReader::Read(is, &positionOffset);
			else
				BinaryReader::ReadIndex(is, &linkBoneIndex, data.header.boneIndexSize);
			if (static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::AppendRotate) ||
				static_cast<uint16_t>(boneFlag) & static_cast<uint16_t>(BoneFlags::AppendTranslate)) {
				BinaryReader::ReadIndex(is, &appendBoneIndex, data.header.boneIndexSize);
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
				BinaryReader::ReadIndex(is, &ikTargetBoneIndex, data.header.boneIndexSize);
				BinaryReader::Read(is, &ikIterationCount);
				BinaryReader::Read(is, &ikLimit);
				int32_t linkCount;
				BinaryReader::Read(is, &linkCount);
				ikLinks.resize(linkCount);
				for (auto& [ikBoneIndex
						 , enableLimit
						 , limitMin
						 , limitMax] : ikLinks) {
					BinaryReader::ReadIndex(is, &ikBoneIndex, data.header.boneIndexSize);
					BinaryReader::Read(is, &enableLimit);
					if (enableLimit != 0) {
						BinaryReader::Read(is, &limitMin);
						BinaryReader::Read(is, &limitMax);
					}
						 }
			}
				 }
	}

	void PmxParser::ReadMorph(std::istream& is) {
		int32_t morphCount;
		BinaryReader::Read(is, &morphCount);
		data.morphs.resize(morphCount);
		for (auto& [name, englishName, controlPanel, morphType
				 , positionMorph, uvMorph, boneMorph
				 , materialMorph, groupMorph
				 , flipMorph, impulseMorph] : data.morphs) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &controlPanel);
			BinaryReader::Read(is, &morphType);
			int32_t dataCount;
			BinaryReader::Read(is, &dataCount);
			if (morphType == MorphType::Position) {
				positionMorph.resize(dataCount);
				for (auto& [vertexIndex, position] : positionMorph) {
					BinaryReader::ReadIndex(is, &vertexIndex, data.header.vertexIndexSize);
					BinaryReader::Read(is, &position);
				}
			} else if (morphType == MorphType::Uv ||
					   morphType == MorphType::AddUv1 ||
					   morphType == MorphType::AddUv2 ||
					   morphType == MorphType::AddUv3 ||
					   morphType == MorphType::AddUv4) {
				uvMorph.resize(dataCount);
				for (auto& [vertexIndex, uv] : uvMorph) {
					BinaryReader::ReadIndex(is, &vertexIndex, data.header.vertexIndexSize);
					BinaryReader::Read(is, &uv);
				}
					   } else if (morphType == MorphType::Bone) {
					   	boneMorph.resize(dataCount);
					   	for (auto& [boneIndex, position, quaternion] : boneMorph) {
					   		BinaryReader::ReadIndex(is, &boneIndex, data.header.boneIndexSize);
					   		BinaryReader::Read(is, &position);
					   		BinaryReader::Read(is, &quaternion);
					   	}
					   } else if (morphType == MorphType::Material) {
					   	materialMorph.resize(dataCount);
					   	for (auto& [materialIndex, opType, diffuse
									, specular, specularPower
									, ambient, edgeColor, edgeSize
									, textureFactor, sphereTextureFactor, toonTextureFactor] : materialMorph) {
					   		BinaryReader::ReadIndex(is, &materialIndex, data.header.materialIndexSize);
					   		BinaryReader::Read(is, &opType);
					   		BinaryReader::Read(is, &diffuse);
					   		BinaryReader::Read(is, &specular);
					   		BinaryReader::Read(is, &specularPower);
					   		BinaryReader::Read(is, &ambient);
					   		BinaryReader::Read(is, &edgeColor);
					   		BinaryReader::Read(is, &edgeSize);
					   		BinaryReader::Read(is, &textureFactor);
					   		BinaryReader::Read(is, &sphereTextureFactor);
					   		BinaryReader::Read(is, &toonTextureFactor);
									}
					   } else if (morphType == MorphType::Group) {
					   	groupMorph.resize(dataCount);
					   	for (auto& [morphIndex, weight] : groupMorph) {
					   		BinaryReader::ReadIndex(is, &morphIndex, data.header.morphIndexSize);
					   		BinaryReader::Read(is, &weight);
					   	}
					   } else if (morphType == MorphType::Flip) {
					   	flipMorph.resize(dataCount);
					   	for (auto& [morphIndex, weight] : flipMorph) {
					   		BinaryReader::ReadIndex(is, &morphIndex, data.header.morphIndexSize);
					   		BinaryReader::Read(is, &weight);
					   	}
					   } else if (morphType == MorphType::Impulse) {
					   	impulseMorph.resize(dataCount);
					   	for (auto& [rigidbodyIndex
									, localFlag
									, translateVelocity
									, rotateTorque] : impulseMorph) {
					   		BinaryReader::ReadIndex(is, &rigidbodyIndex, data.header.rigidbodyIndexSize);
					   		BinaryReader::Read(is, &localFlag);
					   		BinaryReader::Read(is, &translateVelocity);
					   		BinaryReader::Read(is, &rotateTorque);
									}
					   }
				 }
	}

	void PmxParser::ReadDisplayFrame(std::istream& is) {
		int32_t displayFrameCount;
		BinaryReader::Read(is, &displayFrameCount);
		data.displayFrames.resize(displayFrameCount);
		for (auto& [name, englishName
				 , flag, targets] : data.displayFrames) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &flag);
			int32_t targetCount;
			BinaryReader::Read(is, &targetCount);
			targets.resize(targetCount);
			for (auto& [type, index] : targets) {
				BinaryReader::Read(is, &type);
				if (type == TargetType::BoneIndex)
					BinaryReader::ReadIndex(is, &index, data.header.boneIndexSize);
				else if (type == TargetType::MorphIndex)
					BinaryReader::ReadIndex(is, &index, data.header.morphIndexSize);
			}
				 }
	}

	void PmxParser::ReadRigidbody(std::istream& is) {
		int32_t rbCount;
		BinaryReader::Read(is, &rbCount);
		data.rigidBodies.resize(rbCount);
		for (auto& [name, englishName, boneIndex, group, collisionGroup
				 , shape, shapeSize, translate, rotate, mass
				 , translateDimmer, rotateDimmer
				 , repulsion, friction, op] : data.rigidBodies) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::ReadIndex(is, &boneIndex, data.header.boneIndexSize);
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

	void PmxParser::ReadJoint(std::istream& is) {
		int32_t jointCount;
		BinaryReader::Read(is, &jointCount);
		data.joints.resize(jointCount);
		for (auto& [name, englishName, type, rigidbodyAIndex, rigidbodyBIndex
				 , translate, rotate, translateLowerLimit, translateUpperLimit
				 , rotateLowerLimit, rotateUpperLimit
				 , springTranslateFactor, springRotateFactor] : data.joints) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &type);
			BinaryReader::ReadIndex(is, &rigidbodyAIndex, data.header.rigidbodyIndexSize);
			BinaryReader::ReadIndex(is, &rigidbodyBIndex, data.header.rigidbodyIndexSize);
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

	void PmxParser::ReadSoftBody(std::istream& is) {
		int32_t sbCount;
		BinaryReader::Read(is, &sbCount);
		data.softBodies.resize(sbCount);
		for (auto& [name, englishName, type, materialIndex
				 , group, collisionGroup, flag, bodyLinkLength
				 , numClusters, totalMass, collisionMargin, aeroModel
				 , vcf, dp, dg, lf, pr, vc, df, mt
				 , chr, khr, shr, ahr
				 , sRhrCl, sKhrCl, sShrCl
				 , srSplitCl, skSplitCl, ssSplitCl
				 , vIt, pIt, dIt, cIt
				 , lst, ast, vst
				 , anchorRigidBodies, pinVertexIndices] : data.softBodies) {
			ReadString(is, &name);
			ReadString(is, &englishName);
			BinaryReader::Read(is, &type);
			BinaryReader::ReadIndex(is, &materialIndex, data.header.materialIndexSize);
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
				BinaryReader::ReadIndex(is, &rigidBodyIndex, data.header.rigidbodyIndexSize);
				BinaryReader::ReadIndex(is, &vertexIndex, data.header.vertexIndexSize);
				BinaryReader::Read(is, &nearMode);
					 }
			int32_t pvCount;
			BinaryReader::Read(is, &pvCount);
			pinVertexIndices.resize(pvCount);
			for (auto& pv : pinVertexIndices)
				BinaryReader::ReadIndex(is, &pv, data.header.vertexIndexSize);
				 }
	}

	void PmxParser::Clear() {
		data.header = {};
		data.info = {};
		data.vertices.clear();
		data.faces.clear();
		data.textures.clear();
		data.materials.clear();
		data.bones.clear();
		data.morphs.clear();
		data.displayFrames.clear();
		data.rigidBodies.clear();
		data.joints.clear();
		data.softBodies.clear();
	}

	bool PmxParser::ReadFile(const std::filesystem::path& filename) {
		Clear();
		std::ifstream is(filename, std::ios::binary);
		if (!is) {
			std::cerr << "Failed to open PMX file: " << filename.string() << '\n';
			return false;
		}
		const auto Fail = [&](const char* stage) {
			if (is)
				return false;
			std::cerr << "Failed to read PMX " << stage << ": " << filename.string() << '\n';
			Clear();
			return true;
		};
		const auto end = BinaryReader::GetFileEnd(is);
		ReadHeader(is);
		if (Fail("header")) return false;
		ReadInfo(is);
		if (Fail("info")) return false;
		ReadVertex(is);
		if (Fail("vertices")) return false;
		ReadFace(is);
		if (Fail("faces")) return false;
		ReadTexture(is);
		if (Fail("textures")) return false;
		ReadMaterial(is);
		if (Fail("materials")) return false;
		ReadBone(is);
		if (Fail("bones")) return false;
		ReadMorph(is);
		if (Fail("morphs")) return false;
		ReadDisplayFrame(is);
		if (Fail("display frames")) return false;
		ReadRigidbody(is);
		if (Fail("rigid bodies")) return false;
		ReadJoint(is);
		if (Fail("joints")) return false;
		if (BinaryReader::HasMore(is, end)) {
			ReadSoftBody(is);
			if (Fail("soft bodies")) return false;
		}
		return true;
	}
}
