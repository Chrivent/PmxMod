#include "ModelLoader.h"

#include "ModelPose.h"
#include "../Reader/PmxReader.h"
#include "../Util.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Chrivent {
	void ModelLoader::LoadVertices(const PmxReader& pmx, const glm::vec3& invZ) const {
		size_t vertexCount = pmx.vertices.size();
		model.positions.reserve(vertexCount);
		model.normals.reserve(vertexCount);
		model.uvs.reserve(vertexCount);
		model.vertexBoneInfos.reserve(vertexCount);
		model.bboxMax = glm::vec3(-(std::numeric_limits<float>::max)());
		model.bboxMin = glm::vec3((std::numeric_limits<float>::max)());
		for (const auto& v : pmx.vertices) {
			glm::vec3 pos = v.position * invZ;
			model.positions.push_back(pos);
			model.normals.push_back(v.normal * invZ);
			model.uvs.emplace_back(v.uv.x, 1.0f - v.uv.y);
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
			model.vertexBoneInfos.push_back(vtxBoneInfo);
			model.bboxMax = (glm::max)(model.bboxMax, pos);
			model.bboxMin = (glm::min)(model.bboxMin, pos);
		}
		model.morphPositions.resize(model.positions.size());
		model.morphUVs.resize(model.positions.size());
		model.updatePositions.resize(model.positions.size());
		model.updateNormals.resize(model.normals.size());
		model.updateUVs.resize(model.uvs.size());
	}

	bool ModelLoader::LoadFaces(const PmxReader& pmx) const {
		model.indexElementSize = pmx.header.vertexIndexSize;
		model.indices.resize(pmx.faces.size() * 3 * model.indexElementSize);
		model.indexCount = pmx.faces.size() * 3;
		auto FillIndices = [&](auto* out) {
			int idx = 0;
			for (const auto& [tri] : pmx.faces) {
				out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[2]);
				out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[1]);
				out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[0]);
			}
		};
		switch (model.indexElementSize) {
			case 1: FillIndices(reinterpret_cast<uint8_t*>(model.indices.data())); break;
			case 2: FillIndices(reinterpret_cast<uint16_t*>(model.indices.data())); break;
			case 4: FillIndices(reinterpret_cast<uint32_t*>(model.indices.data())); break;
			default: return false;
		}
		return true;
	}

	void ModelLoader::LoadMaterials(
		const PmxReader& pmx,
		const std::filesystem::path& modelDir,
		const std::filesystem::path& dataDir) const {
		std::vector<std::filesystem::path> texturePaths;
		texturePaths.reserve(pmx.textures.size());
		for (const auto& [textureName] : pmx.textures) {
			std::filesystem::path texPath = modelDir / textureName;
			texturePaths.emplace_back(std::move(texPath));
		}
		model.materials.reserve(pmx.materials.size());
		model.subMeshes.reserve(pmx.materials.size());
		uint32_t beginIndex = 0;
		for (const auto& mat : pmx.materials) {
			const auto dm = static_cast<uint8_t>(mat.drawMode);
			Material m;
			m.diffuse = mat.diffuse;
			m.specularPower = mat.specularPower;
			m.specular = mat.specular;
			m.ambient = mat.ambient;
			m.spTextureMode = SphereMode::None;
			m.bothFace = (dm & static_cast<uint8_t>(DrawModeFlags::BothFace)) != 0;
			m.edgeFlag = (dm & static_cast<uint8_t>(DrawModeFlags::DrawEdge)) != 0 ? 1 : 0;
			m.groundShadow = (dm & static_cast<uint8_t>(DrawModeFlags::GroundShadow)) != 0;
			m.shadowCaster = (dm & static_cast<uint8_t>(DrawModeFlags::CastSelfShadow)) != 0;
			m.shadowReceiver = (dm & static_cast<uint8_t>(DrawModeFlags::ReceiveSelfShadow)) != 0;
			m.edgeSize = mat.edgeSize;
			m.edgeColor = mat.edgeColor;
			if (mat.textureIndex != -1)
				m.texture = texturePaths[mat.textureIndex];
			if (mat.cartoonMode == CartoonMode::Common) {
				if (mat.cartoonTextureIndex != -1) {
					std::stringstream ss;
					ss << "cartoon" << std::setfill('0') << std::setw(2) << (mat.cartoonTextureIndex + 1) << ".bmp";
					m.cartoonTexture = dataDir / ss.str();
				}
			} else if (mat.cartoonMode == CartoonMode::Separate) {
				if (mat.cartoonTextureIndex != -1)
					m.cartoonTexture = texturePaths[mat.cartoonTextureIndex];
			}
			if (mat.sphereTextureIndex != -1) {
				m.spTexture = texturePaths[mat.sphereTextureIndex];
				m.spTextureMode = mat.sphereMode;
			}
			model.materials.emplace_back(std::move(m));
			SubMesh subMesh{};
			subMesh.beginIndex = static_cast<int>(beginIndex);
			subMesh.indexCount = mat.numFaceVertices;
			subMesh.materialId = static_cast<int>(model.materials.size() - 1);
			model.subMeshes.push_back(subMesh);
			beginIndex += mat.numFaceVertices;
		}
		model.initMaterials = model.materials;
		model.mulMaterialFactors.resize(model.materials.size());
		model.addMaterialFactors.resize(model.materials.size());
	}

	void ModelLoader::LoadNodes(const PmxReader& pmx, const glm::vec3& invZ) const {
		model.nodes.reserve(pmx.bones.size());
		for (const auto& bone : pmx.bones) {
			auto node = std::make_shared<Node>();
			node->index = static_cast<uint32_t>(model.nodes.size());
			node->name = bone.name;
			model.nodes.emplace_back(std::move(node));
		}
		for (size_t i = 0; i < pmx.bones.size(); i++) {
			const auto& bone = pmx.bones[i];
			auto* node = model.nodes[i].get();
			glm::vec3 localPos = bone.position;
			if (bone.parentBoneIndex != -1) {
				auto parentNode = model.nodes[bone.parentBoneIndex];
				parentNode->AddChild(model.nodes[i]);
				localPos -= pmx.bones[bone.parentBoneIndex].position;
			}
			localPos.z *= -1;
			node->translate = localPos;
			node->global = glm::translate(glm::mat4(1), bone.position * invZ);
			node->inverseInit = glm::inverse(node->global);
			node->deformDepth = bone.deformDepth;
			bool deformAfterPhysics = (static_cast<uint16_t>(bone.boneFlag) & static_cast<uint16_t>(BoneFlags::DeformAfterPhysics)) != 0;
			node->isDeformAfterPhysics = deformAfterPhysics;
			bool appendRotateEnabled = (static_cast<uint16_t>(bone.boneFlag) & static_cast<uint16_t>(BoneFlags::AppendRotate)) != 0;
			bool appendTranslateEnabled = (static_cast<uint16_t>(bone.boneFlag) & static_cast<uint16_t>(BoneFlags::AppendTranslate)) != 0;
			node->isAppendRotate = appendRotateEnabled;
			node->isAppendTranslate = appendTranslateEnabled;
			if ((appendRotateEnabled || appendTranslateEnabled) && bone.appendBoneIndex != -1) {
				bool appendLocalEnabled = (static_cast<uint16_t>(bone.boneFlag) & static_cast<uint16_t>(BoneFlags::AppendLocal)) != 0;
				auto appendNodePtr = model.nodes[bone.appendBoneIndex];
				float appendWeightValue = bone.appendWeight;
				node->isAppendLocal = appendLocalEnabled;
				node->appendNode = appendNodePtr;
				node->appendWeight = appendWeightValue;
			}
			node->initTranslate = node->translate;
			node->initRotate = node->rotate;
			node->initScale = node->scale;
		}
		model.transforms.resize(model.nodes.size());
		model.sortedNodes.clear();
		model.sortedNodes.reserve(model.nodes.size());
		for (auto& node : model.nodes)
			model.sortedNodes.emplace_back(*node);
		std::ranges::stable_sort(model.sortedNodes,
		[](const std::reference_wrapper<Node>& x, const std::reference_wrapper<Node>& y) {
			return x.get().deformDepth < y.get().deformDepth;
		});
		for (size_t i = 0; i < pmx.bones.size(); i++) {
			const auto& bone = pmx.bones[i];
			if (static_cast<uint16_t>(bone.boneFlag) & static_cast<uint16_t>(BoneFlags::Ik)) {
				auto solver = std::make_shared<IkSolver>();
				solver->ikNode = model.nodes[i];
				model.nodes[i]->ikSolver = solver;
				solver->ikTarget = model.nodes[bone.ikTargetBoneIndex];
				for (const auto& [ikBoneIndex, enableLimit, limitMin, limitMax] : bone.ikLinks) {
					auto linkNode = model.nodes[ikBoneIndex];
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
				model.ikSolvers.emplace_back(std::move(solver));
			}
		}
	}

	void ModelLoader::LoadMorphs(const PmxReader& pmx, const glm::vec3& invZ) const {
		for (const auto& morph : pmx.morphs) {
			auto m = std::make_unique<Morph>();
			m->name = morph.name;
			m->morphType = morph.morphType;
			switch (morph.morphType) {
				case MorphType::Position: {
					m->dataIndex = model.positionMorphs.size();
					std::vector<PositionMorph> morphData;
					morphData.reserve(morph.positionMorph.size());
					for (const auto& [vertexIndex, position] : morph.positionMorph)
						morphData.push_back({ vertexIndex, position * invZ });
					model.positionMorphs.emplace_back(std::move(morphData));
					break;
				}
				case MorphType::Uv:
					m->dataIndex = model.uvMorphs.size();
					model.uvMorphs.emplace_back(morph.uvMorph);
					break;
				case MorphType::Material:
					m->dataIndex = model.materialMorphs.size();
					model.materialMorphs.emplace_back(morph.materialMorph);
					break;
				case MorphType::Bone: {
					m->dataIndex = model.boneMorphs.size();
					std::vector<BoneMorph> boneMorphData;
					boneMorphData.reserve(morph.boneMorph.size());
					for (const auto& [boneIndex, position, quaternion] : morph.boneMorph) {
						auto rot = Util::InvZ(glm::mat3_cast(quaternion));
						boneMorphData.push_back({ boneIndex, position * invZ, glm::quat_cast(rot) });
					}
					model.boneMorphs.emplace_back(std::move(boneMorphData));
					break;
				}
				case MorphType::Group:
					m->dataIndex = model.groupMorphs.size();
					model.groupMorphs.emplace_back(morph.groupMorph);
					break;
				default:
					break;
			}
			model.morphs.emplace_back(std::move(m));
		}
	}

	void ModelLoader::FixInfiniteGroupMorphs() const {
		std::vector<int32_t> groupMorphStack;
		std::function<void(int32_t)> fixInfiniteGroupMorph = [&](const int32_t idx) {
			if (idx < 0)
				return;
			const auto* morph = model.morphs[idx].get();
			if (morph->morphType != MorphType::Group)
				return;
			groupMorphStack.push_back(idx);
			for (auto& [morphIndex, weight] : model.groupMorphs[morph->dataIndex]) {
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
		for (int32_t i = 0; i < static_cast<int32_t>(model.morphs.size()); i++) {
			groupMorphStack.clear();
			fixInfiniteGroupMorph(i);
		}
	}

	void ModelLoader::LoadPhysics(const PmxReader& pmx) const {
		model.physics = std::make_unique<Physics>();
		model.physics->Create();
		for (const auto& pmxRigidBody : pmx.rigidBodies) {
			auto rb = std::make_unique<RigidBody>();
			std::shared_ptr<Node> node;
			if (pmxRigidBody.boneIndex != -1)
				node = model.nodes[pmxRigidBody.boneIndex];
			rb->Create(pmxRigidBody, &model, node);
			model.physics->world->addRigidBody(rb->rigidBody.get(), 1 << rb->group, rb->groupMask);
			model.rigidBodies.emplace_back(std::move(rb));
		}
		for (const auto& joint : pmx.joints) {
			if (joint.rigidbodyAIndex != -1 &&
				joint.rigidbodyBIndex != -1 &&
				joint.rigidbodyAIndex != joint.rigidbodyBIndex) {
				auto j = std::make_unique<Joint>();
				j->Create(joint,
					model.rigidBodies[joint.rigidbodyAIndex].get(),
					model.rigidBodies[joint.rigidbodyBIndex].get());
				model.physics->world->addConstraint(j->constraint.get());
				model.joints.emplace_back(std::move(j));
			}
		}
	}

	bool ModelLoader::Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir) const {
		model.Destroy();
		PmxReader pmx;
		if (!pmx.ReadFile(filepath))
			return false;
		model.modelName = pmx.info.modelName;
		model.englishModelName = pmx.info.englishModelName;
		model.comment = pmx.info.comment;
		model.englishComment = pmx.info.englishComment;
		const std::filesystem::path modelDir = filepath.parent_path();
		constexpr glm::vec3 invZ(1, 1, -1);
		LoadVertices(pmx, invZ);
		if (!LoadFaces(pmx))
			return false;
		LoadMaterials(pmx, modelDir, dataDir);
		LoadNodes(pmx, invZ);
		LoadMorphs(pmx, invZ);
		FixInfiniteGroupMorphs();
		LoadPhysics(pmx);
		ModelPose pose(model);
		pose.ResetPhysics();
		pose.SetupParallelUpdate();
		return true;
	}
}
