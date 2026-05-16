#include "ModelLoader.h"

#include "ModelPose.h"
#include "../Util.h"

namespace Chrivent {
	void ModelLoader::LoadVertices(const PmxReader& pmx, const glm::vec3& invZ) const {
		size_t vertexCount = pmx.vertices.size();
		model.geometry.positions.reserve(vertexCount);
		model.geometry.normals.reserve(vertexCount);
		model.geometry.uvs.reserve(vertexCount);
		model.geometry.vertexBoneInfos.reserve(vertexCount);
		model.geometry.bboxMax = glm::vec3(-(std::numeric_limits<float>::max)());
		model.geometry.bboxMin = glm::vec3((std::numeric_limits<float>::max)());
		for (const auto& v : pmx.vertices) {
			glm::vec3 pos = v.position * invZ;
			model.geometry.positions.push_back(pos);
			model.geometry.normals.push_back(v.normal * invZ);
			model.geometry.uvs.emplace_back(v.uv.x, 1.0f - v.uv.y);
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
			model.geometry.vertexBoneInfos.push_back(vtxBoneInfo);
			model.geometry.bboxMax = (glm::max)(model.geometry.bboxMax, pos);
			model.geometry.bboxMin = (glm::min)(model.geometry.bboxMin, pos);
		}
		model.morphData.morphPositions.resize(model.geometry.positions.size());
		model.morphData.morphUVs.resize(model.geometry.positions.size());
		model.geometry.updatePositions.resize(model.geometry.positions.size());
		model.geometry.updateNormals.resize(model.geometry.normals.size());
		model.geometry.updateUVs.resize(model.geometry.uvs.size());
	}

	bool ModelLoader::LoadFaces(const PmxReader& pmx) const {
		model.geometry.indexElementSize = pmx.header.vertexIndexSize;
		model.geometry.indices.resize(pmx.faces.size() * 3 * model.geometry.indexElementSize);
		model.geometry.indexCount = pmx.faces.size() * 3;
		auto FillIndices = [&](auto* out) {
			int idx = 0;
			for (const auto& [tri] : pmx.faces) {
				out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[2]);
				out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[1]);
				out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[0]);
			}
		};
		switch (model.geometry.indexElementSize) {
			case 1: FillIndices(reinterpret_cast<uint8_t*>(model.geometry.indices.data())); break;
			case 2: FillIndices(reinterpret_cast<uint16_t*>(model.geometry.indices.data())); break;
			case 4: FillIndices(reinterpret_cast<uint32_t*>(model.geometry.indices.data())); break;
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
		model.materialData.materials.reserve(pmx.materials.size());
		model.materialData.subMeshes.reserve(pmx.materials.size());
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
			model.materialData.materials.emplace_back(std::move(m));
			SubMesh subMesh{};
			subMesh.beginIndex = static_cast<int>(beginIndex);
			subMesh.indexCount = mat.numFaceVertices;
			subMesh.materialId = static_cast<int>(model.materialData.materials.size() - 1);
			model.materialData.subMeshes.push_back(subMesh);
			beginIndex += mat.numFaceVertices;
		}
		model.materialData.initMaterials = model.materialData.materials;
		model.materialData.mulMaterialFactors.resize(model.materialData.materials.size());
		model.materialData.addMaterialFactors.resize(model.materialData.materials.size());
	}

	void ModelLoader::LoadNodes(const PmxReader& pmx, const glm::vec3& invZ) const {
		model.skeleton.nodes.reserve(pmx.bones.size());
		for (const auto& bone : pmx.bones) {
			auto node = std::make_shared<Node>();
			node->index = static_cast<uint32_t>(model.skeleton.nodes.size());
			node->name = bone.name;
			model.skeleton.nodes.emplace_back(std::move(node));
		}
		for (size_t i = 0; i < pmx.bones.size(); i++) {
			const auto& bone = pmx.bones[i];
			auto* node = model.skeleton.nodes[i].get();
			glm::vec3 localPos = bone.position;
			if (bone.parentBoneIndex != -1) {
				auto parentNode = model.skeleton.nodes[bone.parentBoneIndex];
				parentNode->AddChild(model.skeleton.nodes[i]);
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
				auto appendNodePtr = model.skeleton.nodes[bone.appendBoneIndex];
				float appendWeightValue = bone.appendWeight;
				node->isAppendLocal = appendLocalEnabled;
				node->appendNode = appendNodePtr;
				node->appendWeight = appendWeightValue;
			}
			node->initTranslate = node->translate;
			node->initRotate = node->rotate;
			node->initScale = node->scale;
		}
		model.skeleton.transforms.resize(model.skeleton.nodes.size());
		model.skeleton.sortedNodes.clear();
		model.skeleton.sortedNodes.reserve(model.skeleton.nodes.size());
		for (auto& node : model.skeleton.nodes)
			model.skeleton.sortedNodes.emplace_back(*node);
		std::ranges::stable_sort(model.skeleton.sortedNodes,
		[](const std::reference_wrapper<Node>& x, const std::reference_wrapper<Node>& y) {
			return x.get().deformDepth < y.get().deformDepth;
		});
		for (size_t i = 0; i < pmx.bones.size(); i++) {
			const auto& bone = pmx.bones[i];
			if (static_cast<uint16_t>(bone.boneFlag) & static_cast<uint16_t>(BoneFlags::Ik)) {
				auto solver = std::make_shared<IkSolver>();
				solver->ikNode = model.skeleton.nodes[i];
				model.skeleton.nodes[i]->ikSolver = solver;
				solver->ikTarget = model.skeleton.nodes[bone.ikTargetBoneIndex];
				for (const auto& [ikBoneIndex, enableLimit,
					limitMin, limitMax] : bone.ikLinks) {
					auto linkNode = model.skeleton.nodes[ikBoneIndex];
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
				model.skeleton.ikSolvers.emplace_back(std::move(solver));
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
		for (int32_t i = 0; i < static_cast<int32_t>(model.morphData.morphs.size()); i++) {
			groupMorphStack.clear();
			fixInfiniteGroupMorph(i);
		}
	}

	void ModelLoader::LoadPhysics(const PmxReader& pmx) const {
		model.physicsData.physics = std::make_unique<Physics>();
		model.physicsData.physics->Create();
		for (const auto& pmxRigidBody : pmx.rigidBodies) {
			auto rb = std::make_unique<RigidBody>();
			std::shared_ptr<Node> node;
			if (pmxRigidBody.boneIndex != -1)
				node = model.skeleton.nodes[pmxRigidBody.boneIndex];
			rb->Create(pmxRigidBody, &model, node);
			model.physicsData.physics->world->addRigidBody(rb->rigidBody.get(), 1 << rb->group, rb->groupMask);
			model.physicsData.rigidBodies.emplace_back(std::move(rb));
		}
		for (const auto& joint : pmx.joints) {
			if (joint.rigidbodyAIndex != -1 &&
				joint.rigidbodyBIndex != -1 &&
				joint.rigidbodyAIndex != joint.rigidbodyBIndex) {
				auto j = std::make_unique<Joint>();
				j->Create(joint,
					model.physicsData.rigidBodies[joint.rigidbodyAIndex].get(),
					model.physicsData.rigidBodies[joint.rigidbodyBIndex].get());
				model.physicsData.physics->world->addConstraint(j->constraint.get());
				model.physicsData.joints.emplace_back(std::move(j));
			}
		}
	}

	bool ModelLoader::Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir) const {
		model.Destroy();
		PmxReader pmx;
		if (!pmx.ReadFile(filepath))
			return false;
		model.info.modelName = pmx.info.modelName;
		model.info.englishModelName = pmx.info.englishModelName;
		model.info.comment = pmx.info.comment;
		model.info.englishComment = pmx.info.englishComment;
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
		const ModelPose pose(model);
		pose.ResetPhysics();
		return true;
	}
}
