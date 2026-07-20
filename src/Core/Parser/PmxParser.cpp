#include "Core/Parser/PmxParser.h"

#include "Core/Parser/BinaryReader.h"
#include "Core/Text/TextEncoding.h"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace Chrivent {
	void PmxParser::ReadString(BinaryReader& reader, std::string* val) const {
		int32_t bufferSize = 0;
		if (!reader.ReadCount(bufferSize, 0, 64 * 1024 * 1024))
			return;
		if (bufferSize == 0) {
			val->clear();
			return;
		}
		if (data.header.encodeType == EncodeType::Utf16) {
			if (bufferSize % sizeof(char16_t) != 0) {
				reader.Fail(ParseErrorCode::InvalidValue, "UTF-16 문자열의 바이트 수가 홀수입니다.");
				return;
			}
			std::wstring utf16String(bufferSize / sizeof(char16_t), L'\0');
			if (reader.Read(utf16String.data(), bufferSize)) {
				auto converted = TextEncoding::WideToUtf8(utf16String);
				if (converted.empty()) {
					reader.Fail(ParseErrorCode::InvalidValue, "UTF-16 문자열을 UTF-8로 변환하지 못했습니다.");
					return;
				}
				*val = std::move(converted);
			}
		} else {
			val->resize(bufferSize);
			reader.Read(val->data(), bufferSize);
		}
	}

	void PmxParser::ReadHeader(BinaryReader& reader) {
		reader.Read(data.header.magic, sizeof(data.header.magic));
		reader.Read(data.header.version);
		reader.Read(data.header.dataSize);
		reader.Read(data.header.encodeType);
		reader.Read(data.header.addUvNum);
		reader.Read(data.header.vertexIndexSize);
		reader.Read(data.header.textureIndexSize);
		reader.Read(data.header.materialIndexSize);
		reader.Read(data.header.boneIndexSize);
		reader.Read(data.header.morphIndexSize);
		reader.Read(data.header.rigidbodyIndexSize);
		if (std::memcmp(data.header.magic, "PMX ", sizeof(data.header.magic)) != 0) {
			reader.Fail(ParseErrorCode::InvalidHeader, "PMX 시그니처가 올바르지 않습니다.");
			return;
		}
		if (data.header.version != 2.0f && data.header.version != 2.1f) {
			reader.Fail(ParseErrorCode::UnsupportedVersion, "PMX 2.0 또는 2.1 파일만 지원합니다.");
			return;
		}
		if (data.header.dataSize != 8 || static_cast<uint8_t>(data.header.encodeType) > 1 || data.header.addUvNum > 4) {
			reader.Fail(ParseErrorCode::InvalidHeader, "PMX 전역 설정값이 올바르지 않습니다.");
			return;
		}
		const uint8_t indexSizes[] = {
			data.header.vertexIndexSize, data.header.textureIndexSize, data.header.materialIndexSize,
			data.header.boneIndexSize, data.header.morphIndexSize, data.header.rigidbodyIndexSize
		};
		for (const uint8_t indexSize : indexSizes) {
			if (indexSize != 1 && indexSize != 2 && indexSize != 4) {
				reader.Fail(ParseErrorCode::InvalidHeader, "PMX 인덱스 크기가 올바르지 않습니다.");
				return;
			}
		}
	}

	void PmxParser::ReadInfo(BinaryReader& reader) {
		ReadString(reader, &data.info.modelName);
		ReadString(reader, &data.info.englishModelName);
		ReadString(reader, &data.info.comment);
		ReadString(reader, &data.info.englishComment);
	}

	void PmxParser::ReadVertex(BinaryReader& reader) {
		int32_t vertexCount = 0;
		const std::size_t minimumVertexBytes = sizeof(float) * (8 + 4 * data.header.addUvNum) +
			sizeof(WeightType) +
			data.header.boneIndexSize + sizeof(float);
		if (!reader.ReadCount(vertexCount, minimumVertexBytes, 5'000'000))
			return;
		data.vertices.resize(vertexCount);
		for (auto& [position, normal, uv, addUv,
			weightType, boneIndices, boneWeights,
			sphericalDeformC, sphericalDeformR0, sphericalDeformR1, edgeMag] : data.vertices) {
			reader.Read(position);
			reader.Read(normal);
			reader.Read(uv);
			for (uint8_t i = 0; i < data.header.addUvNum; i++)
				reader.Read(addUv[i]);
			reader.Read(weightType);
			if (weightType > WeightType::QuaternionDeform) {
				reader.Fail(ParseErrorCode::InvalidValue, "지원하지 않는 버텍스 가중치 형식입니다.");
				return;
			}
			switch (weightType) {
				case WeightType::BoneDeform1:
					reader.ReadIndex(boneIndices[0], data.header.boneIndexSize);
					break;
				case WeightType::BoneDeform2:
					reader.ReadIndex(boneIndices[0], data.header.boneIndexSize);
					reader.ReadIndex(boneIndices[1], data.header.boneIndexSize);
					reader.Read(boneWeights[0]);
					break;
				case WeightType::BoneDeform4:
					reader.ReadIndex(boneIndices[0], data.header.boneIndexSize);
					reader.ReadIndex(boneIndices[1], data.header.boneIndexSize);
					reader.ReadIndex(boneIndices[2], data.header.boneIndexSize);
					reader.ReadIndex(boneIndices[3], data.header.boneIndexSize);
					reader.Read(boneWeights[0]);
					reader.Read(boneWeights[1]);
					reader.Read(boneWeights[2]);
					reader.Read(boneWeights[3]);
					break;
				case WeightType::SphericalDeform:
					reader.ReadIndex(boneIndices[0], data.header.boneIndexSize);
					reader.ReadIndex(boneIndices[1], data.header.boneIndexSize);
					reader.Read(boneWeights[0]);
					reader.Read(sphericalDeformC);
					reader.Read(sphericalDeformR0);
					reader.Read(sphericalDeformR1);
					break;
				case WeightType::QuaternionDeform:
					reader.ReadIndex(boneIndices[0], data.header.boneIndexSize);
					reader.ReadIndex(boneIndices[1], data.header.boneIndexSize);
					reader.ReadIndex(boneIndices[2], data.header.boneIndexSize);
					reader.ReadIndex(boneIndices[3], data.header.boneIndexSize);
					reader.Read(boneWeights[0]);
					reader.Read(boneWeights[1]);
					reader.Read(boneWeights[2]);
					reader.Read(boneWeights[3]);
					break;
				default:
					break;
			}
			reader.Read(edgeMag);
		}
	}

	void PmxParser::ReadFace(BinaryReader& reader) {
		int32_t indexCount = 0;
		if (!reader.ReadCount(indexCount, data.header.vertexIndexSize, 15'000'000))
			return;
		if (indexCount % 3 != 0) {
			reader.Fail(ParseErrorCode::InvalidCount, "면 인덱스 개수는 3의 배수여야 합니다.");
			return;
		}
		const int32_t faceCount = indexCount / 3;
		data.faces.resize(faceCount);
		switch (data.header.vertexIndexSize) {
			case 1: {
				std::vector<uint8_t> faceIndices(faceCount * 3);
				reader.Read(faceIndices.data(), faceIndices.size());
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					data.faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					data.faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					data.faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			case 2: {
				std::vector<uint16_t> faceIndices(faceCount * 3);
				reader.Read(faceIndices.data(), faceIndices.size() * sizeof(uint16_t));
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					data.faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					data.faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					data.faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			case 4: {
				std::vector<uint32_t> faceIndices(faceCount * 3);
				reader.Read(faceIndices.data(), faceIndices.size() * sizeof(uint32_t));
				for (int32_t faceIdx = 0; faceIdx < faceCount; faceIdx++) {
					data.faces[faceIdx].vertices[0] = faceIndices[faceIdx * 3 + 0];
					data.faces[faceIdx].vertices[1] = faceIndices[faceIdx * 3 + 1];
					data.faces[faceIdx].vertices[2] = faceIndices[faceIdx * 3 + 2];
				}
			}
				break;
			default:
				break;
		}
	}

	void PmxParser::ReadTexture(BinaryReader& reader) {
		int32_t texCount = 0;
		if (!reader.ReadCount(texCount, sizeof(int32_t), 100'000))
			return;
		data.textures.resize(texCount);
		std::string utf8;
		for (auto& [textureName] : data.textures) {
			ReadString(reader, &utf8);
			const auto* p = reinterpret_cast<const char8_t*>(utf8.data());
			textureName = std::filesystem::path(std::u8string(p, p + utf8.size()));
		}
	}

	void PmxParser::ReadMaterial(BinaryReader& reader) {
		int32_t matCount = 0;
		const std::size_t minimumMaterialBytes = sizeof(int32_t) * 4 + sizeof(float) * 16 +
			sizeof(DrawModeFlags) + data.header.textureIndexSize * 2 +
			sizeof(SphereMode) + sizeof(ToonMode) + sizeof(uint8_t);
		if (!reader.ReadCount(matCount, minimumMaterialBytes, 100'000))
			return;
		data.materials.resize(matCount);
		for (auto& [name, englishName, diffuse, specular,
			specularPower, ambient, drawMode,
			edgeColor, edgeSize,
			textureIndex, sphereTextureIndex,
			sphereMode, toonMode, toonTextureIndex,
			memo, numFaceVertices] : data.materials) {
			ReadString(reader, &name);
			ReadString(reader, &englishName);
			reader.Read(diffuse);
			reader.Read(specular);
			reader.Read(specularPower);
			reader.Read(ambient);
			reader.Read(drawMode);
			reader.Read(edgeColor);
			reader.Read(edgeSize);
			reader.ReadIndex(textureIndex, data.header.textureIndexSize);
			reader.ReadIndex(sphereTextureIndex, data.header.textureIndexSize);
			reader.Read(sphereMode);
			reader.Read(toonMode);
			if (sphereMode > SphereMode::SubTexture || toonMode > ToonMode::Common) {
				reader.Fail(ParseErrorCode::InvalidValue, "재질의 sphere 또는 toon 형식 값이 올바르지 않습니다.");
				return;
			}
			if (toonMode == ToonMode::Separate)
				reader.ReadIndex(toonTextureIndex, data.header.textureIndexSize);
			else if (toonMode == ToonMode::Common) {
				uint8_t toonIndex;
				reader.Read(toonIndex);
				if (toonIndex > 9) {
					reader.Fail(ParseErrorCode::InvalidValue, "공용 툰 텍스처 번호가 올바르지 않습니다.");
					return;
				}
				toonTextureIndex = toonIndex;
			}
			ReadString(reader, &memo);
			reader.Read(numFaceVertices);
		}
	}

	void PmxParser::ReadBone(BinaryReader& reader) {
		int32_t boneCount = 0;
		const std::size_t minimumBoneBytes = sizeof(int32_t) * 3 + sizeof(float) * 3 +
			data.header.boneIndexSize * 2 + sizeof(BoneFlags);
		if (!reader.ReadCount(boneCount, minimumBoneBytes, 500'000))
			return;
		data.bones.resize(boneCount);
		for (auto& [name, englishName, position, parentBoneIndex,
			deformDepth, boneFlag, positionOffset,
			linkBoneIndex, appendBoneIndex, appendWeight,
			fixedAxis, localXAxis, localZAxis, keyValue,
			ikTargetBoneIndex, ikIterationCount,
			ikLimit, ikLinks] : data.bones) {
			ReadString(reader, &name);
			ReadString(reader, &englishName);
			reader.Read(position);
			reader.ReadIndex(parentBoneIndex, data.header.boneIndexSize);
			reader.Read(deformDepth);
			reader.Read(boneFlag);
			if (!ContainsFlag(boneFlag, BoneFlags::TargetShowMode))
				reader.Read(positionOffset);
			else
				reader.ReadIndex(linkBoneIndex, data.header.boneIndexSize);
			if (ContainsFlag(boneFlag, BoneFlags::AppendRotate) ||
				ContainsFlag(boneFlag, BoneFlags::AppendTranslate)) {
				reader.ReadIndex(appendBoneIndex, data.header.boneIndexSize);
				reader.Read(appendWeight);
			}
			if (ContainsFlag(boneFlag, BoneFlags::FixedAxis))
				reader.Read(fixedAxis);
			if (ContainsFlag(boneFlag, BoneFlags::LocalAxis)) {
				reader.Read(localXAxis);
				reader.Read(localZAxis);
			}
			if (ContainsFlag(boneFlag, BoneFlags::DeformOuterParent))
				reader.Read(keyValue);
			if (ContainsFlag(boneFlag, BoneFlags::Ik)) {
				reader.ReadIndex(ikTargetBoneIndex, data.header.boneIndexSize);
				reader.Read(ikIterationCount);
				reader.Read(ikLimit);
				int32_t linkCount = 0;
				if (!reader.ReadCount(linkCount,
					data.header.boneIndexSize + sizeof(uint8_t), 1'000'000))
					return;
				ikLinks.resize(linkCount);
				for (auto& [ikBoneIndex,
					enableLimit,
					limitMin,
					limitMax] : ikLinks) {
					reader.ReadIndex(ikBoneIndex, data.header.boneIndexSize);
					reader.Read(enableLimit);
					if (enableLimit > 1) {
						reader.Fail(ParseErrorCode::InvalidValue, "IK 회전 제한 플래그가 올바르지 않습니다.");
						return;
					}
					if (enableLimit != 0) {
						reader.Read(limitMin);
						reader.Read(limitMax);
					}
				}
			}
		}
	}

	void PmxParser::ReadMorph(BinaryReader& reader) {
		int32_t morphCount = 0;
		constexpr std::size_t minimumMorphBytes = sizeof(int32_t) * 3 +
			sizeof(ControlPanel) + sizeof(MorphType);
		if (!reader.ReadCount(morphCount, minimumMorphBytes, 100'000))
			return;
		data.morphs.resize(morphCount);
		for (auto& [name, englishName, controlPanel, morphType,
			positionMorph, uvMorph, boneMorph,
			materialMorph, groupMorph,
			flipMorph, impulseMorph] : data.morphs) {
			ReadString(reader, &name);
			ReadString(reader, &englishName);
			reader.Read(controlPanel);
			reader.Read(morphType);
			if (static_cast<uint8_t>(controlPanel) > static_cast<uint8_t>(ControlPanel::Other) ||
				static_cast<uint8_t>(morphType) > static_cast<uint8_t>(MorphType::Impulse)) {
				reader.Fail(ParseErrorCode::InvalidValue, "모프 형식 값이 올바르지 않습니다.");
				return;
			}
			int32_t dataCount = 0;
			std::size_t minimumDataBytes = 1;
			switch (morphType) {
				case MorphType::Position:
					minimumDataBytes = data.header.vertexIndexSize + sizeof(float) * 3;
					break;
				case MorphType::Uv:
				case MorphType::AddUv1:
				case MorphType::AddUv2:
				case MorphType::AddUv3:
				case MorphType::AddUv4:
					minimumDataBytes = data.header.vertexIndexSize + sizeof(float) * 4;
					break;
				case MorphType::Bone:
					minimumDataBytes = data.header.boneIndexSize + sizeof(float) * 7;
					break;
				case MorphType::Material:
					minimumDataBytes = data.header.materialIndexSize + sizeof(OpType) + sizeof(float) * 28;
					break;
				case MorphType::Group:
				case MorphType::Flip:
					minimumDataBytes = data.header.morphIndexSize + sizeof(float);
					break;
				case MorphType::Impulse:
					minimumDataBytes = data.header.rigidbodyIndexSize + sizeof(uint8_t) + sizeof(float) * 6;
					break;
			}
			if (!reader.ReadCount(dataCount, minimumDataBytes, 5'000'000))
				return;
			if (morphType == MorphType::Position) {
				positionMorph.resize(dataCount);
				for (auto& [vertexIndex, position] : positionMorph) {
					reader.ReadIndex(vertexIndex, data.header.vertexIndexSize);
					reader.Read(position);
				}
			} else if (morphType == MorphType::Uv ||
					   morphType == MorphType::AddUv1 ||
					   morphType == MorphType::AddUv2 ||
					   morphType == MorphType::AddUv3 ||
					   morphType == MorphType::AddUv4) {
				uvMorph.resize(dataCount);
				for (auto& [vertexIndex, uv] : uvMorph) {
					reader.ReadIndex(vertexIndex, data.header.vertexIndexSize);
					reader.Read(uv);
				}
			} else if (morphType == MorphType::Bone) {
				boneMorph.resize(dataCount);
				for (auto& [boneIndex, position, quaternion] : boneMorph) {
					reader.ReadIndex(boneIndex, data.header.boneIndexSize);
					reader.Read(position);
					reader.Read(quaternion);
				}
			} else if (morphType == MorphType::Material) {
				materialMorph.resize(dataCount);
				for (auto& [materialIndex, opType, diffuse,
					specular, specularPower,
					ambient, edgeColor, edgeSize,
					textureFactor, sphereTextureFactor, toonTextureFactor] : materialMorph) {
					reader.ReadIndex(materialIndex, data.header.materialIndexSize);
					reader.Read(opType);
					reader.Read(diffuse);
					reader.Read(specular);
					reader.Read(specularPower);
					reader.Read(ambient);
					reader.Read(edgeColor);
					reader.Read(edgeSize);
					reader.Read(textureFactor);
					reader.Read(sphereTextureFactor);
					reader.Read(toonTextureFactor);
					if (opType > OpType::Add) {
						reader.Fail(ParseErrorCode::InvalidValue, "재질 모프 연산 형식이 올바르지 않습니다.");
						return;
					}
				}
			} else if (morphType == MorphType::Group) {
				groupMorph.resize(dataCount);
				for (auto& [morphIndex, weight] : groupMorph) {
					reader.ReadIndex(morphIndex, data.header.morphIndexSize);
					reader.Read(weight);
				}
			} else if (morphType == MorphType::Flip) {
				flipMorph.resize(dataCount);
				for (auto& [morphIndex, weight] : flipMorph) {
					reader.ReadIndex(morphIndex, data.header.morphIndexSize);
					reader.Read(weight);
				}
			} else if (morphType == MorphType::Impulse) {
				impulseMorph.resize(dataCount);
				for (auto& [rigidbodyIndex,
					localFlag,
					translateVelocity,
					rotateTorque] : impulseMorph) {
					reader.ReadIndex(rigidbodyIndex, data.header.rigidbodyIndexSize);
					reader.Read(localFlag);
					if (localFlag > 1) {
						reader.Fail(ParseErrorCode::InvalidValue, "임펄스 모프의 로컬 플래그가 올바르지 않습니다.");
						return;
					}
					reader.Read(translateVelocity);
					reader.Read(rotateTorque);
				}
			} else {
				reader.Fail(ParseErrorCode::InvalidValue, "지원하지 않는 모프 형식입니다.");
				return;
			}
		}
	}

	void PmxParser::ReadDisplayFrame(BinaryReader& reader) {
		int32_t displayFrameCount = 0;
		constexpr std::size_t minimumDisplayFrameBytes = sizeof(int32_t) * 3 + sizeof(FrameType);
		if (!reader.ReadCount(displayFrameCount, minimumDisplayFrameBytes, 100'000))
			return;
		data.displayFrames.resize(displayFrameCount);
		for (auto& [name, englishName,
			flag, targets] : data.displayFrames) {
			ReadString(reader, &name);
			ReadString(reader, &englishName);
			reader.Read(flag);
			if (flag != FrameType::DefaultFrame && flag != FrameType::SpecialFrame) {
				reader.Fail(ParseErrorCode::InvalidValue, "표시 프레임 형식이 올바르지 않습니다.");
				return;
			}
			int32_t targetCount = 0;
			const std::size_t minimumTargetBytes = sizeof(TargetType) +
				(std::min)(data.header.boneIndexSize, data.header.morphIndexSize);
			if (!reader.ReadCount(targetCount, minimumTargetBytes, 1'000'000))
				return;
			targets.resize(targetCount);
			for (auto& [type, index] : targets) {
				reader.Read(type);
				if (type == TargetType::BoneIndex)
					reader.ReadIndex(index, data.header.boneIndexSize);
				else if (type == TargetType::MorphIndex)
					reader.ReadIndex(index, data.header.morphIndexSize);
				else {
					reader.Fail(ParseErrorCode::InvalidValue, "표시 프레임 대상 형식이 올바르지 않습니다.");
					return;
				}
			}
		}
	}

	void PmxParser::ReadRigidbody(BinaryReader& reader) {
		int32_t rbCount = 0;
		const std::size_t minimumRigidBodyBytes = sizeof(int32_t) * 2 +
			data.header.boneIndexSize + sizeof(uint8_t) + sizeof(uint16_t) +
			sizeof(Shape) + sizeof(float) * 14 + sizeof(Operation);
		if (!reader.ReadCount(rbCount, minimumRigidBodyBytes, 100'000))
			return;
		data.rigidBodies.resize(rbCount);
		for (auto& [name, englishName, boneIndex, group, collisionGroup,
			shape, shapeSize, translate, rotate, mass,
			translateDimmer, rotateDimmer,
			repulsion, friction, op] : data.rigidBodies) {
			ReadString(reader, &name);
			ReadString(reader, &englishName);
			reader.ReadIndex(boneIndex, data.header.boneIndexSize);
			reader.Read(group);
			reader.Read(collisionGroup);
			reader.Read(shape);
			reader.Read(shapeSize);
			reader.Read(translate);
			reader.Read(rotate);
			reader.Read(mass);
			reader.Read(translateDimmer);
			reader.Read(rotateDimmer);
			reader.Read(repulsion);
			reader.Read(friction);
			reader.Read(op);
			if (shape > Shape::Capsule || op > Operation::DynamicAndBoneMerge) {
				reader.Fail(ParseErrorCode::InvalidValue, "강체 형식 값이 올바르지 않습니다.");
				return;
			}
		}
	}

	void PmxParser::ReadJoint(BinaryReader& reader) {
		int32_t jointCount = 0;
		const std::size_t minimumJointBytes = sizeof(int32_t) * 2 + sizeof(JointType) +
			data.header.rigidbodyIndexSize * 2 + sizeof(float) * 24;
		if (!reader.ReadCount(jointCount, minimumJointBytes, 100'000))
			return;
		data.joints.resize(jointCount);
		for (auto& [name, englishName, type, rigidbodyAIndex, rigidbodyBIndex,
			translate, rotate, translateLowerLimit, translateUpperLimit,
			rotateLowerLimit, rotateUpperLimit,
			springTranslateFactor, springRotateFactor] : data.joints) {
			ReadString(reader, &name);
			ReadString(reader, &englishName);
			reader.Read(type);
			reader.ReadIndex(rigidbodyAIndex, data.header.rigidbodyIndexSize);
			reader.ReadIndex(rigidbodyBIndex, data.header.rigidbodyIndexSize);
			reader.Read(translate);
			reader.Read(rotate);
			reader.Read(translateLowerLimit);
			reader.Read(translateUpperLimit);
			reader.Read(rotateLowerLimit);
			reader.Read(rotateUpperLimit);
			reader.Read(springTranslateFactor);
			reader.Read(springRotateFactor);
			if (type > JointType::Hinge) {
				reader.Fail(ParseErrorCode::InvalidValue, "조인트 형식 값이 올바르지 않습니다.");
				return;
			}
		}
	}

	void PmxParser::ReadSoftBody(BinaryReader& reader) {
		int32_t sbCount = 0;
		if (!reader.ReadCount(sbCount, 128, 10'000))
			return;
		data.softBodies.resize(sbCount);
		for (auto& [name, englishName, type, materialIndex,
			group, collisionGroup, flag, bodyLinkLength,
			numClusters, totalMass, collisionMargin, aeroModel,
			vcf, dp, dg, lf, pr, vc, df, mt,
			chr, khr, shr, ahr,
			sRhrCl, sKhrCl, sShrCl,
			srSplitCl, skSplitCl, ssSplitCl,
			vIt, pIt, dIt, cIt,
			lst, ast, vst,
			anchorRigidBodies, pinVertexIndices] : data.softBodies) {
			ReadString(reader, &name);
			ReadString(reader, &englishName);
			reader.Read(type);
			reader.ReadIndex(materialIndex, data.header.materialIndexSize);
			reader.Read(group);
			reader.Read(collisionGroup);
			reader.Read(flag);
			reader.Read(bodyLinkLength);
			reader.Read(numClusters);
			reader.Read(totalMass);
			reader.Read(collisionMargin);
			reader.Read(aeroModel);
			reader.Read(vcf);
			reader.Read(dp);
			reader.Read(dg);
			reader.Read(lf);
			reader.Read(pr);
			reader.Read(vc);
			reader.Read(df);
			reader.Read(mt);
			reader.Read(chr);
			reader.Read(khr);
			reader.Read(shr);
			reader.Read(ahr);
			reader.Read(sRhrCl);
			reader.Read(sKhrCl);
			reader.Read(sShrCl);
			reader.Read(srSplitCl);
			reader.Read(skSplitCl);
			reader.Read(ssSplitCl);
			reader.Read(vIt);
			reader.Read(pIt);
			reader.Read(dIt);
			reader.Read(cIt);
			reader.Read(lst);
			reader.Read(ast);
			reader.Read(vst);
			if (type > SoftBodyType::Rope) {
				reader.Fail(ParseErrorCode::InvalidValue, "소프트바디 형식 값이 올바르지 않습니다.");
				return;
			}
			int32_t arCount = 0;
			if (!reader.ReadCount(arCount,
				data.header.rigidbodyIndexSize + data.header.vertexIndexSize + 1, 1'000'000))
				return;
			anchorRigidBodies.resize(arCount);
			for (auto& [rigidBodyIndex,
				vertexIndex,
				nearMode] : anchorRigidBodies) {
				reader.ReadIndex(rigidBodyIndex, data.header.rigidbodyIndexSize);
				reader.ReadIndex(vertexIndex, data.header.vertexIndexSize);
				reader.Read(nearMode);
				if (nearMode > 1) {
					reader.Fail(ParseErrorCode::InvalidValue, "소프트바디 앵커의 인접 모드가 올바르지 않습니다.");
					return;
				}
			}
			int32_t pvCount = 0;
			if (!reader.ReadCount(pvCount, data.header.vertexIndexSize, 5'000'000))
				return;
			pinVertexIndices.resize(pvCount);
			for (auto& pv : pinVertexIndices)
				reader.ReadIndex(pv, data.header.vertexIndexSize);
		}
	}

	void PmxParser::ValidateData(BinaryReader& reader) const {
		const auto IsIndexValid = [&](const int32_t index, const std::size_t count, const bool allowMissing = true) {
			return (allowMissing && index == -1) || (index >= 0 && static_cast<std::size_t>(index) < count);
		};
		const auto IsFiniteVector = [](const auto& value) {
			for (glm::length_t index = 0; index < value.length(); index++) {
				if (!std::isfinite(value[index]))
					return false;
			}
			return true;
		};
		for (const auto& [vertices] : data.faces) {
			for (const uint32_t vertexIndex : vertices) {
				if (vertexIndex >= data.vertices.size()) {
					reader.Fail(ParseErrorCode::InvalidIndex, "면이 존재하지 않는 버텍스를 참조합니다.");
					return;
				}
			}
		}
		for (const auto& [position, normal, uv, addUv
			, weightType, boneIndices, boneWeights
			, sphericalDeformC, sphericalDeformR0, sphericalDeformR1, edgeMag] : data.vertices) {
			bool validNumbers = IsFiniteVector(position) && IsFiniteVector(normal) &&
				IsFiniteVector(uv) && std::isfinite(edgeMag);
			for (uint8_t uvIndex = 0; uvIndex < data.header.addUvNum; uvIndex++)
				validNumbers = validNumbers && IsFiniteVector(addUv[uvIndex]);
			if (weightType == WeightType::SphericalDeform) {
				validNumbers = validNumbers && IsFiniteVector(sphericalDeformC) &&
					IsFiniteVector(sphericalDeformR0) && IsFiniteVector(sphericalDeformR1);
			}
			if (!validNumbers) {
				reader.Fail(ParseErrorCode::InvalidValue, "버텍스의 숫자가 올바르지 않습니다.");
				return;
			}
			const uint8_t boneCount = weightType == WeightType::BoneDeform1 ? 1
				: weightType == WeightType::BoneDeform2 || weightType == WeightType::SphericalDeform ? 2 : 4;
			float weights[4] = {
				boneWeights[0],
				boneWeights[1],
				boneWeights[2],
				boneWeights[3]
			};
			if (weightType == WeightType::BoneDeform1)
				weights[0] = 1.0f;
			else if (weightType == WeightType::BoneDeform2 || weightType == WeightType::SphericalDeform)
				weights[1] = 1.0f - weights[0];
			float totalWeight = 0.0f;
			for (uint8_t index = 0; index < boneCount; index++) {
				const float weight = weights[index];
				if (!std::isfinite(weight) || weight < 0.0f || weight > 1.0f) {
					reader.Fail(ParseErrorCode::InvalidValue, "버텍스의 본 가중치가 올바르지 않습니다.");
					return;
				}
				if (boneIndices[index] == -1) {
					if (weight > 1.0e-6f) {
						reader.Fail(ParseErrorCode::InvalidIndex, "가중치가 있는 버텍스가 본을 참조하지 않습니다.");
						return;
					}
				} else if (!IsIndexValid(boneIndices[index], data.bones.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "버텍스가 존재하지 않는 본을 참조합니다.");
					return;
				}
				totalWeight += weight;
			}
			if (!std::isfinite(totalWeight) || std::abs(totalWeight - 1.0f) > 1.0e-3f) {
				reader.Fail(ParseErrorCode::InvalidValue, "버텍스의 본 가중치 합계가 1이 아닙니다.");
				return;
			}
		}
		std::size_t materialIndexCount = 0;
		for (const auto& material : data.materials) {
			if (!IsFiniteVector(material.diffuse) || !IsFiniteVector(material.specular) ||
				!std::isfinite(material.specularPower) || !IsFiniteVector(material.ambient) ||
				!IsFiniteVector(material.edgeColor) || !std::isfinite(material.edgeSize)) {
				reader.Fail(ParseErrorCode::InvalidValue, "재질의 숫자가 올바르지 않습니다.");
				return;
			}
			if (material.numFaceVertices < 0 || material.numFaceVertices % 3 != 0) {
				reader.Fail(ParseErrorCode::InvalidCount, "재질의 면 인덱스 개수가 올바르지 않습니다.");
				return;
			}
			materialIndexCount += static_cast<std::size_t>(material.numFaceVertices);
			if (!IsIndexValid(material.textureIndex, data.textures.size()) ||
				!IsIndexValid(material.sphereTextureIndex, data.textures.size()) ||
				(material.toonMode == ToonMode::Separate && !IsIndexValid(material.toonTextureIndex, data.textures.size())) ||
				material.sphereMode > SphereMode::SubTexture || material.toonMode > ToonMode::Common) {
				reader.Fail(ParseErrorCode::InvalidIndex, "재질의 텍스처 참조 또는 형식 값이 올바르지 않습니다.");
				return;
			}
		}
		if (materialIndexCount != data.faces.size() * 3) {
			reader.Fail(ParseErrorCode::InvalidCount, "재질별 면 인덱스 합계가 전체 면 데이터와 일치하지 않습니다.");
			return;
		}
		for (const auto& bone : data.bones) {
			if (!IsFiniteVector(bone.position) || !IsFiniteVector(bone.positionOffset) ||
				!std::isfinite(bone.appendWeight) || !IsFiniteVector(bone.fixedAxis) ||
				!IsFiniteVector(bone.localXAxis) || !IsFiniteVector(bone.localZAxis) ||
				bone.ikIterationCount < 0 || !std::isfinite(bone.ikLimit)) {
				reader.Fail(ParseErrorCode::InvalidValue, "본 또는 IK의 숫자가 올바르지 않습니다.");
				return;
			}
			if (!IsIndexValid(bone.parentBoneIndex, data.bones.size()) ||
				(ContainsFlag(bone.boneFlag, BoneFlags::TargetShowMode) &&
				 !IsIndexValid(bone.linkBoneIndex, data.bones.size())) ||
				((ContainsFlag(bone.boneFlag, BoneFlags::AppendRotate) ||
				  ContainsFlag(bone.boneFlag, BoneFlags::AppendTranslate)) &&
				 !IsIndexValid(bone.appendBoneIndex, data.bones.size())) ||
				(ContainsFlag(bone.boneFlag, BoneFlags::Ik) &&
				 !IsIndexValid(bone.ikTargetBoneIndex, data.bones.size(), false))) {
				reader.Fail(ParseErrorCode::InvalidIndex, "본 계층 또는 IK 참조가 올바르지 않습니다.");
				return;
			}
			for (const auto& [ikBoneIndex, enableLimit, limitMin, limitMax] : bone.ikLinks) {
				if (!IsIndexValid(ikBoneIndex, data.bones.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "IK 링크가 존재하지 않는 본을 참조합니다.");
					return;
				}
				if (enableLimit != 0 &&
					(!IsFiniteVector(limitMin) || !IsFiniteVector(limitMax))) {
					reader.Fail(ParseErrorCode::InvalidValue, "IK 링크의 회전 제한값이 올바르지 않습니다.");
					return;
				}
			}
		}
		std::vector<uint8_t> boneVisitStates(data.bones.size());
		for (std::size_t boneIndex = 0; boneIndex < data.bones.size(); boneIndex++) {
			int32_t currentIndex = static_cast<int32_t>(boneIndex);
			while (currentIndex != -1 && boneVisitStates[currentIndex] == 0) {
				boneVisitStates[currentIndex] = 1;
				currentIndex = data.bones[currentIndex].parentBoneIndex;
			}
			if (currentIndex != -1 && boneVisitStates[currentIndex] == 1) {
				reader.Fail(ParseErrorCode::InvalidIndex, "본 부모 계층에 순환 참조가 있습니다.");
				return;
			}
			currentIndex = static_cast<int32_t>(boneIndex);
			while (currentIndex != -1 && boneVisitStates[currentIndex] == 1) {
				boneVisitStates[currentIndex] = 2;
				currentIndex = data.bones[currentIndex].parentBoneIndex;
			}
		}
		for (const auto& morph : data.morphs) {
			for (const auto& [vertexIndex, position] : morph.positionMorph) {
				if (!IsIndexValid(vertexIndex, data.vertices.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "위치 모프가 존재하지 않는 버텍스를 참조합니다.");
					return;
				}
				if (!IsFiniteVector(position)) {
					reader.Fail(ParseErrorCode::InvalidValue, "위치 모프의 이동값이 올바르지 않습니다.");
					return;
				}
			}
			for (const auto& [vertexIndex, uv] : morph.uvMorph) {
				if (!IsIndexValid(vertexIndex, data.vertices.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "UV 모프가 존재하지 않는 버텍스를 참조합니다.");
					return;
				}
				if (!IsFiniteVector(uv)) {
					reader.Fail(ParseErrorCode::InvalidValue, "UV 모프의 오프셋이 올바르지 않습니다.");
					return;
				}
			}
			for (const auto& [boneIndex, position, quaternion] : morph.boneMorph) {
				if (!IsIndexValid(boneIndex, data.bones.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "본 모프가 존재하지 않는 본을 참조합니다.");
					return;
				}
				const float quaternionLength = glm::dot(quaternion, quaternion);
				if (!IsFiniteVector(position) || !IsFiniteVector(quaternion) ||
					quaternionLength <= 1.0e-8f) {
					reader.Fail(ParseErrorCode::InvalidValue, "본 모프의 이동 또는 회전값이 올바르지 않습니다.");
					return;
				}
			}
			for (const auto& value : morph.materialMorph) {
				if (!IsIndexValid(value.materialIndex, data.materials.size())) {
					reader.Fail(ParseErrorCode::InvalidIndex, "재질 모프가 존재하지 않는 재질을 참조합니다.");
					return;
				}
				if (!IsFiniteVector(value.diffuse) || !IsFiniteVector(value.specular) ||
					!std::isfinite(value.specularPower) || !IsFiniteVector(value.ambient) ||
					!IsFiniteVector(value.edgeColor) || !std::isfinite(value.edgeSize) ||
					!IsFiniteVector(value.textureFactor) || !IsFiniteVector(value.sphereTextureFactor) ||
					!IsFiniteVector(value.toonTextureFactor)) {
					reader.Fail(ParseErrorCode::InvalidValue, "재질 모프의 숫자가 올바르지 않습니다.");
					return;
				}
			}
			for (const auto& [morphIndex, weight] : morph.groupMorph) {
				if (!IsIndexValid(morphIndex, data.morphs.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "그룹 모프가 존재하지 않는 모프를 참조합니다.");
					return;
				}
				if (!std::isfinite(weight)) {
					reader.Fail(ParseErrorCode::InvalidValue, "그룹 모프의 가중치가 올바르지 않습니다.");
					return;
				}
			}
			for (const auto& [morphIndex, weight] : morph.flipMorph) {
				if (!IsIndexValid(morphIndex, data.morphs.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "플립 모프가 존재하지 않는 모프를 참조합니다.");
					return;
				}
				if (!std::isfinite(weight)) {
					reader.Fail(ParseErrorCode::InvalidValue, "플립 모프의 가중치가 올바르지 않습니다.");
					return;
				}
			}
			for (const auto& value : morph.impulseMorph) {
				if (!IsIndexValid(value.rigidbodyIndex, data.rigidBodies.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "임펄스 모프가 존재하지 않는 강체를 참조합니다.");
					return;
				}
				if (!IsFiniteVector(value.translateVelocity) || !IsFiniteVector(value.rotateTorque)) {
					reader.Fail(ParseErrorCode::InvalidValue, "임펄스 모프의 속도 또는 토크가 올바르지 않습니다.");
					return;
				}
			}
		}
		for (const auto& frame : data.displayFrames) {
			for (const auto& [type, index] : frame.targets) {
				const bool valid = type == TargetType::BoneIndex
					? IsIndexValid(index, data.bones.size(), false)
					: IsIndexValid(index, data.morphs.size(), false);
				if (!valid) {
					reader.Fail(ParseErrorCode::InvalidIndex, "표시 프레임 대상 인덱스가 올바르지 않습니다.");
					return;
				}
			}
		}
		for (const auto& rigidBody : data.rigidBodies) {
			if (!IsFiniteVector(rigidBody.shapeSize) || !IsFiniteVector(rigidBody.translate) ||
				!IsFiniteVector(rigidBody.rotate) || !std::isfinite(rigidBody.mass) ||
				!std::isfinite(rigidBody.translateDimmer) || !std::isfinite(rigidBody.rotateDimmer) ||
				!std::isfinite(rigidBody.repulsion) || !std::isfinite(rigidBody.friction)) {
				reader.Fail(ParseErrorCode::InvalidValue, "강체의 숫자가 올바르지 않습니다.");
				return;
			}
			if (!IsIndexValid(rigidBody.boneIndex, data.bones.size())) {
				reader.Fail(ParseErrorCode::InvalidIndex, "강체가 존재하지 않는 본을 참조합니다.");
				return;
			}
			if (rigidBody.group > 15) {
				reader.Fail(ParseErrorCode::InvalidValue, "강체 충돌 그룹은 0부터 15 사이여야 합니다.");
				return;
			}
		}
		for (const auto& joint : data.joints) {
			if (!IsFiniteVector(joint.translate) || !IsFiniteVector(joint.rotate) ||
				!IsFiniteVector(joint.translateLowerLimit) || !IsFiniteVector(joint.translateUpperLimit) ||
				!IsFiniteVector(joint.rotateLowerLimit) || !IsFiniteVector(joint.rotateUpperLimit) ||
				!IsFiniteVector(joint.springTranslateFactor) || !IsFiniteVector(joint.springRotateFactor)) {
				reader.Fail(ParseErrorCode::InvalidValue, "조인트의 숫자가 올바르지 않습니다.");
				return;
			}
			if (!IsIndexValid(joint.rigidbodyAIndex, data.rigidBodies.size()) ||
				!IsIndexValid(joint.rigidbodyBIndex, data.rigidBodies.size())) {
				reader.Fail(ParseErrorCode::InvalidIndex, "조인트가 존재하지 않는 강체를 참조합니다.");
				return;
			}
		}
		for (const auto& softBody : data.softBodies) {
			const float coefficients[] = {
				softBody.totalMass, softBody.collisionMargin,
				softBody.vcf, softBody.dp, softBody.dg, softBody.lf, softBody.pr, softBody.vc,
				softBody.df, softBody.mt, softBody.chr, softBody.khr, softBody.shr, softBody.ahr,
				softBody.sRhrCl, softBody.sKhrCl, softBody.sShrCl,
				softBody.srSplitCl, softBody.skSplitCl, softBody.ssSplitCl,
				softBody.lst, softBody.ast, softBody.vst
			};
			bool validCoefficients = true;
			for (const float coefficient : coefficients)
				validCoefficients = validCoefficients && std::isfinite(coefficient);
			if (!validCoefficients || softBody.bodyLinkLength < 0 || softBody.numClusters < 0 ||
				softBody.vIt < 0 || softBody.pIt < 0 || softBody.dIt < 0 || softBody.cIt < 0 ||
				softBody.aeroModel < static_cast<int32_t>(AeroModel::KAeroModelVTwoSided) ||
				softBody.aeroModel > static_cast<int32_t>(AeroModel::KAeroModelFOneSided)) {
				reader.Fail(ParseErrorCode::InvalidValue, "소프트바디의 숫자 또는 형식 값이 올바르지 않습니다.");
				return;
			}
			if (!IsIndexValid(softBody.materialIndex, data.materials.size(), false)) {
				reader.Fail(ParseErrorCode::InvalidIndex, "소프트바디가 존재하지 않는 재질을 참조합니다.");
				return;
			}
			for (const auto& anchor : softBody.anchorRigidBodies) {
				if (!IsIndexValid(anchor.rigidBodyIndex, data.rigidBodies.size(), false) ||
					!IsIndexValid(anchor.vertexIndex, data.vertices.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "소프트바디 앵커 참조가 올바르지 않습니다.");
					return;
				}
			}
			for (const int32_t vertexIndex : softBody.pinVertexIndices) {
				if (!IsIndexValid(vertexIndex, data.vertices.size(), false)) {
					reader.Fail(ParseErrorCode::InvalidIndex, "소프트바디 핀 버텍스 참조가 올바르지 않습니다.");
					return;
				}
			}
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

	std::expected<void, ParseError> PmxParser::Read(std::istream& stream) {
		Clear();
		BinaryReader reader(stream);
		const auto ReadSection = [&](const char* section, auto read) {
			reader.SetSection(section);
			read();
			return reader.Result().has_value();
		};
		if (!ReadSection("header", [&] { ReadHeader(reader); }) ||
			!ReadSection("info", [&] { ReadInfo(reader); }) ||
			!ReadSection("vertices", [&] { ReadVertex(reader); }) ||
			!ReadSection("faces", [&] { ReadFace(reader); }) ||
			!ReadSection("textures", [&] { ReadTexture(reader); }) ||
			!ReadSection("materials", [&] { ReadMaterial(reader); }) ||
			!ReadSection("bones", [&] { ReadBone(reader); }) ||
			!ReadSection("morphs", [&] { ReadMorph(reader); }) ||
			!ReadSection("display frames", [&] { ReadDisplayFrame(reader); }) ||
			!ReadSection("rigid bodies", [&] { ReadRigidbody(reader); }) ||
			!ReadSection("joints", [&] { ReadJoint(reader); })) {
			const auto result = reader.Result();
			Clear();
			return result;
		}
		if ((data.header.version == 2.1f || reader.HasMore()) &&
			!ReadSection("soft bodies", [&] { ReadSoftBody(reader); })) {
			const auto result = reader.Result();
			Clear();
			return result;
		}
		reader.SetSection("references");
		ValidateData(reader);
		const auto result = reader.Result();
		if (!result)
			Clear();
		return result;
	}

	std::expected<void, ParseError> PmxParser::ReadFile(const std::filesystem::path& filename) {
		std::ifstream stream(filename, std::ios::binary);
		if (!stream) {
			Clear();
			return std::unexpected(ParseError{
				ParseErrorCode::FileOpen, "file", "PMX 파일을 열 수 없습니다: " + filename.string(), 0
			});
		}
		return Read(stream);
	}
}
