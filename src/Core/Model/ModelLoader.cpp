#include "Core/Model/ModelLoader.h"

#include "Core/Model/ModelPose.h"
#include "Core/Parser/PmxParser.h"
#include "Util.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace Chrivent {
	void ModelLoader::LoadVertices(const PmxParser::PmxData& pmxData, const glm::vec3& invZ) const {
		size_t vertexCount = pmxData.vertices.size();
		model.geometryData.positions.reserve(vertexCount);
		model.geometryData.normals.reserve(vertexCount);
		model.geometryData.uvs.reserve(vertexCount);
		model.geometryData.vertexBoneInfos.reserve(vertexCount);
		model.geometryData.bboxMax = glm::vec3(-std::numeric_limits<float>::max());
		model.geometryData.bboxMin = glm::vec3(std::numeric_limits<float>::max());
		for (const auto& v : pmxData.vertices) {
			glm::vec3 pos = v.position * invZ;
			model.geometryData.positions.push_back(pos);
			model.geometryData.normals.push_back(v.normal * invZ);
			model.geometryData.uvs.emplace_back(v.uv.x, 1.0f - v.uv.y);
			Vertex vtxBoneInfo{};
			if (WeightType::SphericalDeform != v.weightType) {
				vtxBoneInfo.boneIndices[0] = v.boneIndices[0];
				vtxBoneInfo.boneIndices[1] = v.boneIndices[1];
				vtxBoneInfo.boneIndices[2] = v.boneIndices[2];
				vtxBoneInfo.boneIndices[3] = v.boneIndices[3];
				vtxBoneInfo.boneWeights[0] = v.boneWeights[0];
				vtxBoneInfo.boneWeights[1] = v.boneWeights[1];
				vtxBoneInfo.boneWeights[2] = v.boneWeights[2];
				vtxBoneInfo.boneWeights[3] = v.boneWeights[3];
			}
			vtxBoneInfo.weightType = v.weightType;
			switch (v.weightType) {
				case WeightType::BoneDeform2:
					vtxBoneInfo.boneWeights[1] = 1.0f - vtxBoneInfo.boneWeights[0];
					break;
				case WeightType::SphericalDeform: {
					auto w0 = v.boneWeights[0];
					auto w1 = 1.0f - w0;
					auto center = v.sphericalDeformC * invZ;
					auto r0 = v.sphericalDeformR0 * invZ;
					auto r1 = v.sphericalDeformR1 * invZ;
					auto rw = r0 * w0 + r1 * w1;
					r0 = center + r0 - rw;
					r1 = center + r1 - rw;
					auto cr0 = (center + r0) * 0.5f;
					auto cr1 = (center + r1) * 0.5f;
					vtxBoneInfo.boneIndices[0] = v.boneIndices[0];
					vtxBoneInfo.boneIndices[1] = v.boneIndices[1];
					vtxBoneInfo.boneWeights[0] = v.boneWeights[0];
					vtxBoneInfo.sphericalDeformC = center;
					vtxBoneInfo.sphericalDeformR0 = cr0;
					vtxBoneInfo.sphericalDeformR1 = cr1;
				}
					break;
				default:
					break;
			}
			model.geometryData.vertexBoneInfos.push_back(vtxBoneInfo);
			model.geometryData.bboxMax = (glm::max)(model.geometryData.bboxMax, pos);
			model.geometryData.bboxMin = (glm::min)(model.geometryData.bboxMin, pos);
		}
		model.morphData.morphPositions.resize(model.geometryData.positions.size());
		model.morphData.morphUVs.resize(model.geometryData.positions.size());
		model.geometryData.updatePositions.resize(model.geometryData.positions.size());
		model.geometryData.updateNormals.resize(model.geometryData.normals.size());
		model.geometryData.updateUVs.resize(model.geometryData.uvs.size());
	}

	bool ModelLoader::LoadFaces(const PmxParser::PmxData& pmxData) const {
		model.geometryData.indexElementSize = pmxData.header.vertexIndexSize;
		model.geometryData.indices.resize(pmxData.faces.size() * 3 * model.geometryData.indexElementSize);
		model.geometryData.indexCount = pmxData.faces.size() * 3;
		auto FillIndices = [&](auto* out) {
			int idx = 0;
			for (const auto& [tri] : pmxData.faces) {
				out[idx++] = tri[2];
				out[idx++] = tri[1];
				out[idx++] = tri[0];
			}
		};
		switch (model.geometryData.indexElementSize) {
			case 1: FillIndices(reinterpret_cast<uint8_t*>(model.geometryData.indices.data())); break;
			case 2: FillIndices(reinterpret_cast<uint16_t*>(model.geometryData.indices.data())); break;
			case 4: FillIndices(reinterpret_cast<uint32_t*>(model.geometryData.indices.data())); break;
			default: return false;
		}
		return true;
	}

	void ModelLoader::LoadMaterials(
		const PmxParser::PmxData& pmxData,
		const std::filesystem::path& modelDir,
		const std::filesystem::path& dataDir) const {
		std::vector<std::filesystem::path> texturePaths;
		texturePaths.reserve(pmxData.textures.size());
		for (const auto& [textureName] : pmxData.textures) {
			std::filesystem::path texPath = modelDir / textureName;
			texturePaths.emplace_back(std::move(texPath));
		}
		model.materialData.materials.reserve(pmxData.materials.size());
		model.materialData.subMeshes.reserve(pmxData.materials.size());
		size_t beginIndex = 0;
		for (const auto& mat : pmxData.materials) {
			Material m;
			m.diffuse = mat.diffuse;
			m.specularPower = mat.specularPower;
			m.specular = mat.specular;
			m.ambient = mat.ambient;
			m.spTextureMode = SphereMode::None;
			m.bothFace = Util::HasFlag(mat.drawMode, DrawModeFlags::BothFace);
			m.edgeFlag = Util::HasFlag(mat.drawMode, DrawModeFlags::DrawEdge) ? 1 : 0;
			m.groundShadow = Util::HasFlag(mat.drawMode, DrawModeFlags::GroundShadow);
			m.shadowCaster = Util::HasFlag(mat.drawMode, DrawModeFlags::CastSelfShadow);
			m.shadowReceiver = Util::HasFlag(mat.drawMode, DrawModeFlags::ReceiveSelfShadow);
			m.edgeSize = mat.edgeSize;
			m.edgeColor = mat.edgeColor;
			if (mat.textureIndex != -1)
				m.texture = texturePaths[mat.textureIndex];
			if (mat.toonMode == ToonMode::Common) {
				if (mat.toonTextureIndex != -1) {
					std::stringstream ss;
					ss << "toon" << std::setfill('0') << std::setw(2) << (mat.toonTextureIndex + 1) << ".bmp";
					m.toonTexture = dataDir / ss.str();
				}
			} else if (mat.toonMode == ToonMode::Separate) {
				if (mat.toonTextureIndex != -1)
					m.toonTexture = texturePaths[mat.toonTextureIndex];
			}
			if (mat.sphereTextureIndex != -1) {
				m.spTexture = texturePaths[mat.sphereTextureIndex];
				m.spTextureMode = mat.sphereMode;
			}
			model.materialData.materials.emplace_back(std::move(m));
			SubMesh subMesh{};
			subMesh.beginIndex = beginIndex;
			subMesh.indexCount = mat.numFaceVertices;
			subMesh.materialId = model.materialData.materials.size() - 1;
			model.materialData.subMeshes.push_back(subMesh);
			beginIndex += subMesh.indexCount;
		}
		model.materialData.initMaterials = model.materialData.materials;
		model.materialData.mulMaterialFactors.resize(model.materialData.materials.size());
		model.materialData.addMaterialFactors.resize(model.materialData.materials.size());
	}

	void ModelLoader::LoadNodes(const PmxParser::PmxData& pmxData, const glm::vec3& invZ) const {
		model.skeletonData.nodes.reserve(pmxData.bones.size());
		for (const auto& bone : pmxData.bones) {
			auto node = std::make_shared<Node>();
			node->index = model.skeletonData.nodes.size();
			node->name = bone.name;
			model.skeletonData.nodes.emplace_back(std::move(node));
		}
		for (size_t i = 0; i < pmxData.bones.size(); i++) {
			const auto& bone = pmxData.bones[i];
			auto* node = model.skeletonData.nodes[i].get();
			glm::vec3 localPos = bone.position;
			if (bone.parentBoneIndex != -1) {
				auto parentNode = model.skeletonData.nodes[bone.parentBoneIndex];
				parentNode->AddChild(model.skeletonData.nodes[i]);
				localPos -= pmxData.bones[bone.parentBoneIndex].position;
			}
			localPos.z *= -1;
			node->translate = localPos;
			node->global = glm::translate(glm::mat4(1), bone.position * invZ);
			node->inverseInit = glm::inverse(node->global);
			node->deformDepth = bone.deformDepth;
			bool deformAfterPhysics = Util::HasFlag(bone.boneFlag, BoneFlags::DeformAfterPhysics);
			node->isDeformAfterPhysics = deformAfterPhysics;
			bool appendRotateEnabled = Util::HasFlag(bone.boneFlag, BoneFlags::AppendRotate);
			bool appendTranslateEnabled = Util::HasFlag(bone.boneFlag, BoneFlags::AppendTranslate);
			node->isAppendRotate = appendRotateEnabled;
			node->isAppendTranslate = appendTranslateEnabled;
			if ((appendRotateEnabled || appendTranslateEnabled) && bone.appendBoneIndex != -1) {
				bool appendLocalEnabled = Util::HasFlag(bone.boneFlag, BoneFlags::AppendLocal);
				auto appendNodePtr = model.skeletonData.nodes[bone.appendBoneIndex];
				float appendWeightValue = bone.appendWeight;
				node->isAppendLocal = appendLocalEnabled;
				node->appendNode = appendNodePtr;
				node->appendWeight = appendWeightValue;
			}
			node->initTranslate = node->translate;
			node->initRotate = node->rotate;
			node->initScale = node->scale;
		}
		model.skeletonData.transforms.resize(model.skeletonData.nodes.size());
		model.skeletonData.sortedNodes.clear();
		model.skeletonData.sortedNodes.reserve(model.skeletonData.nodes.size());
		for (auto& node : model.skeletonData.nodes)
			model.skeletonData.sortedNodes.emplace_back(*node);
		std::ranges::stable_sort(model.skeletonData.sortedNodes,
		[](const std::reference_wrapper<Node>& x, const std::reference_wrapper<Node>& y) {
			return x.get().deformDepth < y.get().deformDepth;
		});
		for (size_t i = 0; i < pmxData.bones.size(); i++) {
			const auto& bone = pmxData.bones[i];
			if (Util::HasFlag(bone.boneFlag, BoneFlags::Ik)) {
				auto solver = std::make_shared<IkSolver>();
				solver->ikNode = model.skeletonData.nodes[i];
				model.skeletonData.nodes[i]->ikSolver = solver;
				solver->ikTarget = model.skeletonData.nodes[bone.ikTargetBoneIndex];
				for (const auto& [ikBoneIndex, enableLimit,
					limitMin, limitMax] : bone.ikLinks) {
					auto linkNode = model.skeletonData.nodes[ikBoneIndex];
					IkChain chain{};
					chain.node = linkNode;
					chain.enableAxisLimit = enableLimit;
					chain.limitMin = limitMax * glm::vec3(-1);
					chain.limitMax = limitMin * glm::vec3(-1);
					chain.saveIkRot = glm::quat(1, 0, 0, 0);
					solver->chains.emplace_back(chain);
					linkNode->enableIk = true;
				}
				solver->iterateCount = bone.ikIterationCount;
				solver->limitAngle = bone.ikLimit;
				model.skeletonData.ikSolvers.emplace_back(std::move(solver));
			}
		}
		model.skeletonData.displayFrames.reserve(pmxData.displayFrames.size());
		for (const auto& displayFrame : pmxData.displayFrames) {
			ModelDisplayFrame frame;
			frame.name = displayFrame.name.empty() ? displayFrame.englishName : displayFrame.name;
			for (const auto& [type, index] : displayFrame.targets) {
				if (index < 0)
					continue;
				if (type == TargetType::BoneIndex && index < pmxData.bones.size())
					frame.boneIndices.emplace_back(index);
				else if (type == TargetType::MorphIndex && index < pmxData.morphs.size())
					frame.morphIndices.emplace_back(index);
			}
			model.skeletonData.displayFrames.emplace_back(std::move(frame));
		}
	}

	void ModelLoader::LoadMorphs(const PmxParser::PmxData& pmxData, const glm::vec3& invZ) const {
		for (const auto& morph : pmxData.morphs) {
			auto m = std::make_unique<Morph>();
			m->name = morph.name;
			m->morphType = morph.morphType;
			switch (morph.morphType) {
				case MorphType::Position: {
					m->dataIndex = model.morphData.positionMorphs.size();
					std::vector<PositionMorph> morphData;
					morphData.reserve(morph.positionMorph.size());
					for (const auto& [vertexIndex, position] : morph.positionMorph)
						morphData.push_back({ vertexIndex, position * invZ });
					model.morphData.positionMorphs.emplace_back(std::move(morphData));
					break;
				}
				case MorphType::Uv:
					m->dataIndex = model.morphData.uvMorphs.size();
					model.morphData.uvMorphs.emplace_back(morph.uvMorph);
					break;
				case MorphType::Material:
					m->dataIndex = model.morphData.materialMorphs.size();
					model.morphData.materialMorphs.emplace_back(morph.materialMorph);
					break;
				case MorphType::Bone: {
					m->dataIndex = model.morphData.boneMorphs.size();
					std::vector<BoneMorph> boneMorphData;
					boneMorphData.reserve(morph.boneMorph.size());
					for (const auto& [boneIndex, position, quaternion] : morph.boneMorph) {
						auto rot = Util::InvZ(glm::mat3_cast(quaternion));
						boneMorphData.push_back({ boneIndex, position * invZ, glm::quat_cast(rot) });
					}
					model.morphData.boneMorphs.emplace_back(std::move(boneMorphData));
					break;
				}
				case MorphType::Group:
					m->dataIndex = model.morphData.groupMorphs.size();
					model.morphData.groupMorphs.emplace_back(morph.groupMorph);
					break;
				default:
					break;
			}
			model.morphData.morphs.emplace_back(std::move(m));
		}
	}

	void ModelLoader::FixInfiniteGroupMorphs() const {
		std::vector<int32_t> groupMorphStack;
		std::function<void(int32_t)> fixInfiniteGroupMorph = [&](const int32_t idx) {
			if (idx < 0)
				return;
			const auto* morph = model.morphData.morphs[idx].get();
			if (morph->morphType != MorphType::Group)
				return;
			groupMorphStack.push_back(idx);
			for (auto& [morphIndex, weight] : model.morphData.groupMorphs[morph->dataIndex]) {
				if (morphIndex < 0)
					continue;
				if (std::ranges::find(groupMorphStack, morphIndex) != groupMorphStack.end()) {
					morphIndex = -1;
					continue;
				}
				fixInfiniteGroupMorph(morphIndex);
			}
			groupMorphStack.pop_back();
		};
		for (int32_t i = 0; i < model.morphData.morphs.size(); i++) {
			groupMorphStack.clear();
			fixInfiniteGroupMorph(i);
		}
	}

	void ModelLoader::LoadPhysics(const PmxParser::PmxData& pmxData) const {
		if (pmxData.rigidBodies.empty())
			return;
		model.physicsData.physics = std::make_unique<Physics>();
		model.physicsData.physics->Create();
		for (const auto& pmxRigidBody : pmxData.rigidBodies) {
			auto rb = std::make_unique<RigidBody>();
			std::shared_ptr<Node> node;
			if (pmxRigidBody.boneIndex != -1)
				node = model.skeletonData.nodes[pmxRigidBody.boneIndex];
			rb->Create(pmxRigidBody, &model, node);
			model.physicsData.physics->world->addRigidBody(
				rb->rigidBody.get(), 1 << rb->group, rb->groupMask);
			model.physicsData.rigidBodies.emplace_back(std::move(rb));
		}
		for (const auto& joint : pmxData.joints) {
			if (joint.rigidbodyAIndex != -1 &&
				joint.rigidbodyBIndex != -1 &&
				joint.rigidbodyAIndex != joint.rigidbodyBIndex) {
				auto j = std::make_unique<Joint>();
				j->Create(joint,
					*model.physicsData.rigidBodies[joint.rigidbodyAIndex],
					*model.physicsData.rigidBodies[joint.rigidbodyBIndex]);
				model.physicsData.physics->world->addConstraint(j->GetConstraint());
				model.physicsData.joints.emplace_back(std::move(j));
			}
		}
	}

	bool ModelLoader::Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir) const {
		model.Reset();
		PmxParser pmx;
		const auto parseResult = pmx.ReadFile(filepath);
		if (!parseResult) {
			std::cerr << "Failed to read PMX file: " << FormatParseError(parseResult.error()) << '\n';
			return false;
		}
		const auto& pmxData = pmx.GetData();
		model.infoData.modelName = pmxData.info.modelName;
		model.infoData.englishModelName = pmxData.info.englishModelName;
		model.infoData.comment = pmxData.info.comment;
		model.infoData.englishComment = pmxData.info.englishComment;
		const std::filesystem::path modelDir = filepath.parent_path();
		constexpr glm::vec3 invZ(1, 1, -1);
		LoadVertices(pmxData, invZ);
		if (!LoadFaces(pmxData))
			return false;
		LoadMaterials(pmxData, modelDir, dataDir);
		LoadNodes(pmxData, invZ);
		LoadMorphs(pmxData, invZ);
		FixInfiniteGroupMorphs();
		LoadPhysics(pmxData);
		const ModelPose pose(model);
		pose.ResetPhysics();
		return true;
	}
}
