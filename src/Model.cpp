#include "Model.h"

#include "Animation.h"
#include "Util.h"

#include <ranges>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

void Mul(MaterialMorph& out, const MaterialMorph& val, const float weight) {
	out.m_diffuse = glm::mix(out.m_diffuse, out.m_diffuse * val.m_diffuse, weight);
	out.m_specular = glm::mix(out.m_specular, out.m_specular * val.m_specular, weight);
	out.m_specularPower = glm::mix(out.m_specularPower, out.m_specularPower * val.m_specularPower, weight);
	out.m_ambient = glm::mix(out.m_ambient, out.m_ambient * val.m_ambient, weight);
	out.m_edgeColor = glm::mix(out.m_edgeColor, out.m_edgeColor * val.m_edgeColor, weight);
	out.m_edgeSize = glm::mix(out.m_edgeSize, out.m_edgeSize * val.m_edgeSize, weight);
	out.m_textureFactor = glm::mix(out.m_textureFactor, out.m_textureFactor * val.m_textureFactor, weight);
	out.m_sphereTextureFactor = glm::mix(out.m_sphereTextureFactor, out.m_sphereTextureFactor * val.m_sphereTextureFactor, weight);
	out.m_toonTextureFactor = glm::mix(out.m_toonTextureFactor, out.m_toonTextureFactor * val.m_toonTextureFactor, weight);
}

void Add(MaterialMorph& out, const MaterialMorph& val, const float weight) {
	out.m_diffuse += val.m_diffuse * weight;
	out.m_specular += val.m_specular * weight;
	out.m_specularPower += val.m_specularPower * weight;
	out.m_ambient += val.m_ambient * weight;
	out.m_edgeColor += val.m_edgeColor * weight;
	out.m_edgeSize += val.m_edgeSize * weight;
	out.m_textureFactor += val.m_textureFactor * weight;
	out.m_sphereTextureFactor += val.m_sphereTextureFactor * weight;
	out.m_toonTextureFactor += val.m_toonTextureFactor * weight;
}

Model::~Model() {
	Destroy();
}

void Model::InitializeAnimation() {
	ClearBaseAnimation();
	for (const auto& node : nodes) {
		node->animTranslate = glm::vec3(0);
		node->animRotate = glm::quat(1, 0, 0, 0);
	}
	BeginAnimation();
	for (const auto& morph : morphs)
		morph->weight = 0;
	for (const auto& ikSolver : ikSolvers)
		ikSolver->enable = true;
	UpdateNodeAnimation(false);
	UpdateNodeAnimation(true);
	ResetPhysics();
}

void Model::SaveBaseAnimation() const {
	for (const auto& node : nodes) {
		node->baseAnimTranslate = node->animTranslate;
		node->baseAnimRotate = node->animRotate;
	}
	for (const auto& morph : morphs)
		morph->saveAnimWeight = morph->weight;
	for (const auto& ikSolver : ikSolvers)
		ikSolver->baseAnimEnable = ikSolver->enable;
}

void Model::ClearBaseAnimation() const {
	for (const auto& node : nodes) {
		node->baseAnimTranslate = glm::vec3(0);
		node->baseAnimRotate = glm::quat(1, 0, 0, 0);
	}
	for (const auto& morph : morphs)
		morph->saveAnimWeight = 0;
	for (const auto& ikSolver : ikSolvers)
		ikSolver->baseAnimEnable = true;
}

void Model::BeginAnimation() {
	for (const auto& node : nodes)
		node->BeginUpdateTransform();
	for (const auto& node : nodes) {
		node->animTranslate = glm::vec3(0);
		node->animRotate = glm::quat(1, 0, 0, 0);
	}
	std::ranges::fill(m_morphPositions, glm::vec3(0));
	std::ranges::fill(m_morphUVs, glm::vec4(0));
}

void Model::UpdateMorphAnimation() {
	BeginMorphMaterial();
	for (const auto& morph : morphs)
		EvalMorph(morph.get(), morph->weight);
	EndMorphMaterial();
}

void Model::UpdateNodeAnimation(const bool afterPhysicsAnim) const {
	const auto pred = [&](const std::reference_wrapper<Node>& node) {
		return node.get().isDeformAfterPhysics == afterPhysicsAnim;
	};
	for (auto& nodeRef : m_sortedNodes | std::views::filter(pred))
		nodeRef.get().UpdateLocalTransform();
	for (auto& nodeRef : m_sortedNodes | std::views::filter(pred)) {
		auto& node = nodeRef.get();
		if (node.parent.expired())
			node.UpdateGlobalTransform();
	}
	for (auto& nodeRef : m_sortedNodes | std::views::filter(pred)) {
		auto& node = nodeRef.get();
		if (!node.appendNode.expired()) {
			node.UpdateAppendTransform();
			node.UpdateGlobalTransform();
		}
		if (const auto ikSolver = node.ikSolver.lock()) {
			ikSolver->Solve();
			node.UpdateGlobalTransform();
		}
	}
}

void Model::ResetPhysics() const {
	for (auto& rb : m_rigidBodies) {
		rb->ApplyActivation(false);
		rb->ResetTransform();
	}
	m_physics->m_world->stepSimulation(
		1.0f / 60.0f, m_physics->m_maxSubStepCount,
		static_cast<btScalar>(1.0f / m_physics->m_fps));
	for (auto& rb : m_rigidBodies) {
		rb->ReflectGlobalTransform();
		rb->CalcLocalTransform();
	}
	for (const auto& node : nodes) {
		if (node->parent.expired())
			node->UpdateGlobalTransform();
	}
	for (auto& rb : m_rigidBodies)
		rb->Reset(m_physics.get());
}

void Model::UpdatePhysicsAnimation(const float elapsed) const {
	for (auto& rb : m_rigidBodies)
		rb->ApplyActivation(true);
	m_physics->m_world->stepSimulation(
		elapsed, m_physics->m_maxSubStepCount,
		static_cast<btScalar>(1.0f / m_physics->m_fps));
	for (auto& rb : m_rigidBodies) {
		rb->ReflectGlobalTransform();
		rb->CalcLocalTransform();
	}
	for (const auto& node : nodes) {
		if (node->parent.expired())
			node->UpdateGlobalTransform();
	}
}

void Model::Update() {
	for (size_t i = 0; i < nodes.size(); i++)
		m_transforms[i] = nodes[i]->global * nodes[i]->inverseInit;
	if (m_parallelUpdateCount != m_updateRanges.size())
		SetupParallelUpdate();
	const size_t futureCount = m_parallelUpdateFutures.size();
	for (size_t i = 0; i < futureCount; i++) {
		if (m_updateRanges[i + 1].vertexCount != 0) {
			m_parallelUpdateFutures[i] = std::async(std::launch::async,
			[this, range = m_updateRanges[i + 1]] { this->Update(range); }
			);
		}
	}
	Update(m_updateRanges[0]);
	for (size_t i = 0; i < futureCount; i++) {
		if (m_updateRanges[i + 1].vertexCount != 0)
			m_parallelUpdateFutures[i].wait();
	}
}

void Model::UpdateAllAnimation(const Animation* anim, const float frame, const float physicsElapsed) {
	if (anim)
		anim->Evaluate(frame);
	UpdateMorphAnimation();
	UpdateNodeAnimation(false);
	UpdatePhysicsAnimation(physicsElapsed);
	UpdateNodeAnimation(true);
}

bool Model::Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir) {
	Destroy();
	PMXReader pmx;
	if (!pmx.ReadFile(filepath))
		return false;
	m_modelName        = pmx.m_info.m_modelName;
	m_englishModelName = pmx.m_info.m_englishModelName;
	m_comment          = pmx.m_info.m_comment;
	m_englishComment   = pmx.m_info.m_englishComment;
	std::filesystem::path dirPath = filepath.parent_path();
	size_t vertexCount = pmx.m_vertices.size();
	positions.reserve(vertexCount);
	m_normals.reserve(vertexCount);
	m_uvs.reserve(vertexCount);
	m_vertexBoneInfos.reserve(vertexCount);
	m_bboxMax = glm::vec3(-std::numeric_limits<float>::max());
	m_bboxMin = glm::vec3(std::numeric_limits<float>::max());
	constexpr glm::vec3 invZ(1, 1, -1);
	for (const auto& v : pmx.m_vertices) {
		glm::vec3 pos = v.m_position * invZ;
		positions.push_back(pos);
		m_normals.push_back(v.m_normal * invZ);
		m_uvs.emplace_back(v.m_uv.x, 1.0f - v.m_uv.y);
		Vertex vtxBoneInfo{};
		if (WeightType::SDEF != v.m_weightType) {
			vtxBoneInfo.boneIndices[0] = v.m_boneIndices[0];
			vtxBoneInfo.boneIndices[1] = v.m_boneIndices[1];
			vtxBoneInfo.boneIndices[2] = v.m_boneIndices[2];
			vtxBoneInfo.boneIndices[3] = v.m_boneIndices[3];
			vtxBoneInfo.boneWeights[0] = v.m_boneWeights[0];
			vtxBoneInfo.boneWeights[1] = v.m_boneWeights[1];
			vtxBoneInfo.boneWeights[2] = v.m_boneWeights[2];
			vtxBoneInfo.boneWeights[3] = v.m_boneWeights[3];
		}
		vtxBoneInfo.weightType = v.m_weightType;
		switch (v.m_weightType) {
			case WeightType::BDEF2:
				vtxBoneInfo.boneWeights[1] = 1.0f - vtxBoneInfo.boneWeights[0];
				break;
			case WeightType::SDEF: {
					auto w0 = v.m_boneWeights[0];
					auto w1 = 1.0f - w0;
					auto center = v.m_sdefC * invZ;
					auto r0 = v.m_sdefR0 * invZ;
					auto r1 = v.m_sdefR1 * invZ;
					auto rw = r0 * w0 + r1 * w1;
					r0 = center + r0 - rw;
					r1 = center + r1 - rw;
					auto cr0 = (center + r0) * 0.5f;
					auto cr1 = (center + r1) * 0.5f;
					vtxBoneInfo.boneIndices[0] = v.m_boneIndices[0];
					vtxBoneInfo.boneIndices[1] = v.m_boneIndices[1];
					vtxBoneInfo.boneWeights[0] = v.m_boneWeights[0];
					vtxBoneInfo.sdefC = center;
					vtxBoneInfo.sdefR0 = cr0;
					vtxBoneInfo.sdefR1 = cr1;
				}
				break;
			default:
				break;
		}
		m_vertexBoneInfos.push_back(vtxBoneInfo);
		m_bboxMax = glm::max(m_bboxMax, pos);
		m_bboxMin = glm::min(m_bboxMin, pos);
	}
	m_morphPositions.resize(positions.size());
	m_morphUVs.resize(positions.size());
	updatePositions.resize(positions.size());
	updateNormals.resize(m_normals.size());
	updateUVs.resize(m_uvs.size());
	indexElementSize = pmx.m_header.m_vertexIndexSize;
	indices.resize(pmx.m_faces.size() * 3 * indexElementSize);
	indexCount = pmx.m_faces.size() * 3;
	auto fillIndices = [&](auto* out) {
		using T = std::remove_pointer_t<decltype(out)>;
		int idx = 0;
		for (const auto& [tri] : pmx.m_faces) {
			out[idx++] = static_cast<T>(tri[2]);
			out[idx++] = static_cast<T>(tri[1]);
			out[idx++] = static_cast<T>(tri[0]);
		}
	};
	switch (indexElementSize) {
		case 1: fillIndices(reinterpret_cast<uint8_t*>(indices.data())); break;
		case 2: fillIndices(reinterpret_cast<uint16_t*>(indices.data())); break;
		case 4: fillIndices(reinterpret_cast<uint32_t*>(indices.data())); break;
		default: return false;
	}
	std::vector<std::filesystem::path> texturePaths;
	texturePaths.reserve(pmx.m_textures.size());
	for (const auto& [m_textureName] : pmx.m_textures) {
		std::filesystem::path texPath = dirPath / m_textureName;
		texturePaths.emplace_back(std::move(texPath));
	}
	materials.reserve(pmx.m_materials.size());
	subMeshes.reserve(pmx.m_materials.size());
	uint32_t beginIndex = 0;
	for (const auto& mat : pmx.m_materials) {
		const auto dm = static_cast<uint8_t>(mat.m_drawMode);
		Material m;
		m.diffuse = mat.m_diffuse;
		m.specularPower = mat.m_specularPower;
		m.specular = mat.m_specular;
		m.ambient = mat.m_ambient;
		m.spTextureMode = SphereMode::None;
		m.bothFace      = (dm & static_cast<uint8_t>(DrawModeFlags::BothFace)) != 0;
		m.edgeFlag      = (dm & static_cast<uint8_t>(DrawModeFlags::DrawEdge)) != 0 ? 1 : 0;
		m.groundShadow  = (dm & static_cast<uint8_t>(DrawModeFlags::GroundShadow)) != 0;
		m.shadowCaster  = (dm & static_cast<uint8_t>(DrawModeFlags::CastSelfShadow)) != 0;
		m.shadowReceiver= (dm & static_cast<uint8_t>(DrawModeFlags::ReceiveSelfShadow)) != 0;
		m.edgeSize = mat.m_edgeSize;
		m.edgeColor = mat.m_edgeColor;
		if (mat.m_textureIndex != -1)
			m.texture = texturePaths[mat.m_textureIndex];
		if (mat.m_toonMode == ToonMode::Common) {
			if (mat.m_toonTextureIndex != -1) {
				std::stringstream ss;
				ss << "toon" << std::setfill('0') << std::setw(2) << (mat.m_toonTextureIndex + 1) << ".bmp";
				m.toonTexture = dataDir / ss.str();
			}
		} else if (mat.m_toonMode == ToonMode::Separate) {
			if (mat.m_toonTextureIndex != -1)
				m.toonTexture = texturePaths[mat.m_toonTextureIndex];
		}
		if (mat.m_sphereTextureIndex != -1) {
			m.spTexture = texturePaths[mat.m_sphereTextureIndex];
			m.spTextureMode = mat.m_sphereMode;
		}
		materials.emplace_back(std::move(m));
		SubMesh subMesh{};
		subMesh.beginIndex = static_cast<int>(beginIndex);
		subMesh.indexCount = mat.m_numFaceVertices;
		subMesh.materialID = static_cast<int>(materials.size() - 1);
		subMeshes.push_back(subMesh);
		beginIndex += mat.m_numFaceVertices;
	}
	m_initMaterials = materials;
	m_mulMaterialFactors.resize(materials.size());
	m_addMaterialFactors.resize(materials.size());
	nodes.reserve(pmx.m_bones.size());
	for (const auto& bone : pmx.m_bones) {
		auto node = std::make_shared<Node>();
		node->index = static_cast<uint32_t>(nodes.size());
		node->name = bone.m_name;
		nodes.emplace_back(std::move(node));
	}
	for (size_t i = 0; i < pmx.m_bones.size(); i++) {
		const auto& bone = pmx.m_bones[i];
		auto* node = nodes[i].get();
		glm::vec3 localPos = bone.m_position;
		if (bone.m_parentBoneIndex != -1) {
			auto parentNode = nodes[bone.m_parentBoneIndex];
			parentNode->AddChild(nodes[i]);
			localPos -= pmx.m_bones[bone.m_parentBoneIndex].m_position;
		}
		localPos.z *= -1;
		node->translate = localPos;
		node->global = glm::translate(glm::mat4(1), bone.m_position * invZ);
		node->inverseInit = glm::inverse(node->global);
		node->deformDepth = bone.m_deformDepth;
		bool deformAfterPhysics = (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::DeformAfterPhysics)) != 0;
		node->isDeformAfterPhysics = deformAfterPhysics;
		bool appendRotateEnabled = (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::AppendRotate)) != 0;
		bool appendTranslateEnabled = (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::AppendTranslate)) != 0;
		node->isAppendRotate = appendRotateEnabled;
		node->isAppendTranslate = appendTranslateEnabled;
		if ((appendRotateEnabled || appendTranslateEnabled) && bone.m_appendBoneIndex != -1) {
			bool appendLocalEnabled = (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::AppendLocal)) != 0;
			auto appendNodePtr = nodes[bone.m_appendBoneIndex];
			float appendWeightValue = bone.m_appendWeight;
			node->isAppendLocal = appendLocalEnabled;
			node->appendNode = appendNodePtr;
			node->appendWeight = appendWeightValue;
		}
		node->initTranslate = node->translate;
		node->initRotate = node->rotate;
		node->initScale = node->scale;
	}
	m_transforms.resize(nodes.size());
	m_sortedNodes.clear();
	m_sortedNodes.reserve(nodes.size());
	for (auto& node : nodes)
		m_sortedNodes.emplace_back(*node);
	std::ranges::stable_sort(m_sortedNodes,
		[](const std::reference_wrapper<Node>& x, const std::reference_wrapper<Node>& y) {
			return x.get().deformDepth < y.get().deformDepth;
		}
	);
	for (size_t i = 0; i < pmx.m_bones.size(); i++) {
		const auto& bone = pmx.m_bones[i];
		if (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::IK)) {
			auto solver = std::make_shared<IkSolver>();
			solver->ikNode = nodes[i];
			nodes[i]->ikSolver = solver;
			solver->ikTarget = nodes[bone.m_ikTargetBoneIndex];
			for (const auto& [m_ikBoneIndex, m_enableLimit, m_limitMin, m_limitMax] : bone.m_ikLinks) {
				auto linkNode = nodes[m_ikBoneIndex];
				IKChain chain{};
				chain.node = linkNode;
				chain.enableAxisLimit = m_enableLimit;
				chain.limitMin = m_limitMax * glm::vec3(-1);
				chain.limitMax = m_limitMin * glm::vec3(-1);
				chain.saveIKRot = glm::quat(1, 0, 0, 0);
				solver->chains.emplace_back(chain);
				linkNode->enableIK = true;
			}
			solver->iterateCount = bone.m_ikIterationCount;
			solver->limitAngle = bone.m_ikLimit;
			ikSolvers.emplace_back(std::move(solver));
		}
	}
	for (const auto& morph : pmx.m_morphs) {
		auto m = std::make_unique<Morph>();
		m->name = morph.m_name;
		m->weight = 0.0f;
		m->morphType = morph.m_morphType;
		if (morph.m_morphType == MorphType::Position) {
			m->dataIndex = m_positionMorphDatas.size();
			std::vector<PositionMorph> morphData;
			for (const auto& [m_vertexIndex, m_position] : morph.m_positionMorph) {
				PositionMorph morphVtx{};
				morphVtx.m_vertexIndex = m_vertexIndex;
				morphVtx.m_position = m_position * invZ;
				morphData.push_back(morphVtx);
			}
			m_positionMorphDatas.emplace_back(std::move(morphData));
		} else if (morph.m_morphType == MorphType::UV) {
			m->dataIndex = m_uvMorphDatas.size();
			std::vector<UVMorph> morphData;
			for (const auto& [m_vertexIndex, m_uv] : morph.m_uvMorph) {
				UVMorph morphUV{};
				morphUV.m_vertexIndex = m_vertexIndex;
				morphUV.m_uv = m_uv;
				morphData.push_back(morphUV);
			}
			m_uvMorphDatas.emplace_back(std::move(morphData));
		} else if (morph.m_morphType == MorphType::Material) {
			m->dataIndex = m_materialMorphDatas.size();
			m_materialMorphDatas.emplace_back(morph.m_materialMorph);
		} else if (morph.m_morphType == MorphType::Bone) {
			m->dataIndex = m_boneMorphDatas.size();
			std::vector<BoneMorph> boneMorphData;
			for (const auto& [m_boneIndex, m_position, m_quaternion] : morph.m_boneMorph) {
				auto rot = Util::InvZ(glm::mat3_cast(m_quaternion));
				BoneMorph boneMorphElem{};
				boneMorphElem.m_boneIndex = m_boneIndex;
				boneMorphElem.m_position = m_position * invZ;
				boneMorphElem.m_quaternion = glm::quat_cast(rot);
				boneMorphData.push_back(boneMorphElem);
			}
			m_boneMorphDatas.emplace_back(boneMorphData);
		} else if (morph.m_morphType == MorphType::Group) {
			m->dataIndex = m_groupMorphDatas.size();
			m_groupMorphDatas.emplace_back(morph.m_groupMorph);
		}
		morphs.emplace_back(std::move(m));
	}
	std::vector<int32_t> groupMorphStack;
	std::function<void(int32_t)> fixInfiniteGroupMorph = [&](const int32_t idx) {
		if (idx < 0)
			return;
		const auto* morph = morphs[idx].get();
		if (morph->morphType != MorphType::Group)
			return;
		groupMorphStack.push_back(idx);
		for (auto& [childIdx, w] : m_groupMorphDatas[morph->dataIndex]) {
			if (childIdx < 0)
				continue;
			if (std::ranges::find(groupMorphStack, childIdx) != groupMorphStack.end()) {
				childIdx = -1;
				continue;
			}
			fixInfiniteGroupMorph(childIdx);
		}
		groupMorphStack.pop_back();
	};
	for (int32_t i = 0; i < static_cast<int32_t>(morphs.size()); i++) {
		groupMorphStack.clear();
		fixInfiniteGroupMorph(i);
	}
	m_physics = std::make_unique<Physics>();
	m_physics->Create();
	for (const auto& rigidBody : pmx.m_rigidBodies) {
		auto rb = std::make_unique<RigidBody>();
		std::shared_ptr<Node> node;
		if (rigidBody.m_boneIndex != -1)
			node = nodes[rigidBody.m_boneIndex];
		rb->Create(rigidBody, this, node);
		m_physics->m_world->addRigidBody(rb->m_rigidBody.get(), 1 << rb->m_group, rb->m_groupMask);
		m_rigidBodies.emplace_back(std::move(rb));
	}
	for (const auto& joint : pmx.m_joints) {
		if (joint.m_rigidbodyAIndex != -1 &&
		    joint.m_rigidbodyBIndex != -1 &&
		    joint.m_rigidbodyAIndex != joint.m_rigidbodyBIndex) {
			auto j = std::make_unique<Joint>();
			j->Create(joint,
				m_rigidBodies[joint.m_rigidbodyAIndex].get(),
				m_rigidBodies[joint.m_rigidbodyBIndex].get()
			);
			m_physics->m_world->addConstraint(j->m_constraint.get());
			m_joints.emplace_back(std::move(j));
		}
	}
	ResetPhysics();
	SetupParallelUpdate();
	return true;
}

void Model::Destroy() {
	materials.clear();
	subMeshes.clear();
	positions.clear();
	m_normals.clear();
	m_uvs.clear();
	m_vertexBoneInfos.clear();
	indices.clear();
	m_sortedNodes.clear();
	nodes.clear();
	m_updateRanges.clear();
	for (const auto& joint : m_joints)
		m_physics->m_world->removeConstraint(joint->m_constraint.get());
	m_joints.clear();
	for (const auto& rb : m_rigidBodies)
		m_physics->m_world->removeRigidBody(rb->m_rigidBody.get());
	m_rigidBodies.clear();
	m_physics.reset();
}

void Model::SetupParallelUpdate() {
	if (!m_parallelUpdateCount)
		m_parallelUpdateCount = std::max(1u, std::thread::hardware_concurrency());
	m_parallelUpdateCount = std::min<size_t>(m_parallelUpdateCount, 16);
	m_updateRanges.resize(m_parallelUpdateCount);
	m_parallelUpdateFutures.resize(m_parallelUpdateCount - 1);
	const size_t vertexCount = positions.size();
	constexpr size_t LowerVertexCount = 1000;
	if (vertexCount < m_updateRanges.size() * LowerVertexCount) {
		const size_t numRanges = (vertexCount + LowerVertexCount - 1) / LowerVertexCount;
		for (size_t i = 0; i < m_updateRanges.size(); i++) {
			auto& [rangeVertexOffset, rangeVertexCount] = m_updateRanges[i];
			if (i < numRanges) {
				rangeVertexOffset = i * LowerVertexCount;
				rangeVertexCount  = std::min(LowerVertexCount, vertexCount - rangeVertexOffset);
			} else {
				rangeVertexOffset = 0;
				rangeVertexCount = 0;
			}
		}
		return;
	}
	const size_t numVertexCount = vertexCount / m_updateRanges.size();
	size_t offset = 0;
	for (size_t i = 0; i < m_updateRanges.size(); i++) {
		auto& [rangeVertexOffset, rangeVertexCount] = m_updateRanges[i];
		rangeVertexOffset = offset;
		rangeVertexCount  = numVertexCount + (i == 0 ? vertexCount % m_updateRanges.size() : 0);
		offset += rangeVertexCount;
	}
}

void Model::Update(const UpdateRange& range) {
	const auto* position = positions.data() + range.vertexOffset;
	const auto* normal = m_normals.data() + range.vertexOffset;
	const auto* uv = m_uvs.data() + range.vertexOffset;
	const auto* morphPos = m_morphPositions.data() + range.vertexOffset;
	const auto* morphUV = m_morphUVs.data() + range.vertexOffset;
	const auto* vtxInfo = m_vertexBoneInfos.data() + range.vertexOffset;
	const auto* transforms = m_transforms.data();
	auto* updatePos = updatePositions.data() + range.vertexOffset;
	auto* updateNormal = updateNormals.data() + range.vertexOffset;
	auto* updateUV = updateUVs.data() + range.vertexOffset;
	for (size_t i = 0; i < range.vertexCount;
		i++, vtxInfo++, position++, normal++, uv++, morphPos++, morphUV++, updatePos++, updateNormal++, updateUV++) {
		glm::mat4 m;
		switch (vtxInfo->weightType) {
			case WeightType::BDEF1: {
				m = transforms[vtxInfo->boneIndices[0]];
				break;
			}
			case WeightType::BDEF2: {
				const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
				const auto w0 = vtxInfo->boneWeights[0], w1 = vtxInfo->boneWeights[1];
				m = transforms[i0] * w0 + transforms[i1] * w1;
				break;
			}
			case WeightType::BDEF4: {
				const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
				const auto i2 = vtxInfo->boneIndices[2], i3 = vtxInfo->boneIndices[3];
				const auto w0 = vtxInfo->boneWeights[0], w1 = vtxInfo->boneWeights[1];
				const auto w2 = vtxInfo->boneWeights[2], w3 = vtxInfo->boneWeights[3];
				m = transforms[i0] * w0 + transforms[i1] * w1 + transforms[i2] * w2 + transforms[i3] * w3;
				break;
			}
			case WeightType::SDEF: {
				const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
				const auto w0 = vtxInfo->boneWeights[0], w1 = 1.0f - w0;
				const auto center = vtxInfo->sdefC, cr0 = vtxInfo->sdefR0, cr1 = vtxInfo->sdefR1;
				const auto q0 = glm::quat_cast(nodes[i0]->global);
				const auto q1 = glm::quat_cast(nodes[i1]->global);
				const auto rot_mat = glm::mat3_cast(glm::slerp(q0, q1, w1));
				const auto m0 = transforms[i0], m1 = transforms[i1];
				const auto pos = *position + *morphPos;
				*updatePos = rot_mat * (pos - center)
				+ glm::vec3(m0 * glm::vec4(cr0, 1)) * w0
				+ glm::vec3(m1 * glm::vec4(cr1, 1)) * w1;
				*updateNormal = rot_mat * *normal;
				break;
			}
			case WeightType::QDEF: {
				glm::dualquat dq[4]{};
				float w[4] = {};
				for (int bi = 0; bi < 4; bi++) {
					auto boneID = vtxInfo->boneIndices[bi];
					if (boneID != -1) {
						dq[bi] = glm::normalize(glm::dualquat_cast(glm::mat3x4(glm::transpose(transforms[boneID]))));
						w[bi] = vtxInfo->boneWeights[bi];
					}
				}
				if (glm::dot(dq[0].real, dq[1].real) < 0)
					w[1] *= -1.0f;
				if (glm::dot(dq[0].real, dq[2].real) < 0)
					w[2] *= -1.0f;
				if (glm::dot(dq[0].real, dq[3].real) < 0)
					w[3] *= -1.0f;
				auto blendDQ = glm::normalize(w[0] * dq[0] + w[1] * dq[1] + w[2] * dq[2] + w[3] * dq[3]);
				m = glm::transpose(glm::mat3x4_cast(blendDQ));
				break;
			}
			default:
				break;
		}
		if (WeightType::SDEF != vtxInfo->weightType) {
			*updatePos = glm::vec3(m * glm::vec4(*position + *morphPos, 1));
			*updateNormal = glm::normalize(glm::mat3(m) * *normal);
		}
		*updateUV = *uv + glm::vec2(morphUV->x, morphUV->y);
	}
}

void Model::EvalMorph(const Morph* morph, const float weight) {
	if (weight == 0)
		return;
	switch (morph->morphType) {
		case MorphType::Position:
			MorphPosition(m_positionMorphDatas[morph->dataIndex], weight);
			break;
		case MorphType::UV:
			MorphUV(m_uvMorphDatas[morph->dataIndex], weight);
			break;
		case MorphType::Material:
			MorphMaterial(m_materialMorphDatas[morph->dataIndex], weight);
			break;
		case MorphType::Bone:
			MorphBone(m_boneMorphDatas[morph->dataIndex], weight);
			break;
		case MorphType::Group: {
			for (const auto& [m_morphIndex, m_weight] : m_groupMorphDatas[morph->dataIndex]) {
				if (m_morphIndex == -1)
					continue;
				EvalMorph(morphs[m_morphIndex].get(), m_weight * weight);
			}
			break;
		}
		default:
			break;
	}
}

void Model::MorphPosition(const std::vector<PositionMorph>& morphData, const float weight) {
	for (const auto& [m_index, m_position] : morphData)
		m_morphPositions[m_index] += m_position * weight;
}

void Model::MorphUV(const std::vector<UVMorph>& morphData, const float weight) {
	for (const auto& [m_index, m_uv] : morphData)
		m_morphUVs[m_index] += m_uv * weight;
}

void Model::BeginMorphMaterial() {
	constexpr MaterialMorph initMul{
		0, OpType::Mul, glm::vec4(1), glm::vec3(1), 1.0f, glm::vec3(1),
		glm::vec4(1), 1.0f, glm::vec4(1), glm::vec4(1), glm::vec4(1)
	};
	constexpr MaterialMorph initAdd{
		0, OpType::Add, glm::vec4(0), glm::vec3(0), 0.0f, glm::vec3(0),
		glm::vec4(0), 0.0f, glm::vec4(0), glm::vec4(0), glm::vec4(0)
	};
	for (size_t i = 0; i < materials.size(); i++) {
		auto& mul = m_mulMaterialFactors[i];
		mul = initMul;
		mul.m_diffuse       = m_initMaterials[i].diffuse;
		mul.m_specular      = m_initMaterials[i].specular;
		mul.m_specularPower = m_initMaterials[i].specularPower;
		mul.m_ambient       = m_initMaterials[i].ambient;
		m_addMaterialFactors[i] = initAdd;
	}
}

void Model::EndMorphMaterial() {
	for (size_t i = 0; i < materials.size(); i++) {
		auto& mat = materials[i];
		const auto& mul = m_mulMaterialFactors[i];
		const auto& add = m_addMaterialFactors[i];
		auto matFactor = mul;
		Add(matFactor, add, 1.0f);
		mat.diffuse        = matFactor.m_diffuse;
		mat.specular       = matFactor.m_specular;
		mat.specularPower  = matFactor.m_specularPower;
		mat.ambient        = matFactor.m_ambient;
		mat.textureMulFactor   = mul.m_textureFactor;
		mat.textureAddFactor   = add.m_textureFactor;
		mat.spTextureMulFactor = mul.m_sphereTextureFactor;
		mat.spTextureAddFactor = add.m_sphereTextureFactor;
		mat.toonTextureMulFactor = mul.m_toonTextureFactor;
		mat.toonTextureAddFactor = add.m_toonTextureFactor;
	}
}

void Model::MorphMaterial(const std::vector<MaterialMorph>& morphData, const float weight) {
	for (const auto& matMorph : morphData) {
		auto apply = [&](const size_t mi) {
			switch (matMorph.m_opType) {
				case OpType::Mul: Mul(m_mulMaterialFactors[mi], matMorph, weight); break;
				case OpType::Add: Add(m_addMaterialFactors[mi], matMorph, weight); break;
				default: break;
			}
		};
		if (matMorph.m_materialIndex != -1) {
			apply(static_cast<size_t>(matMorph.m_materialIndex));
		} else {
			for (size_t i = 0; i < materials.size(); i++)
				apply(i);
		}
	}
}

void Model::MorphBone(const std::vector<BoneMorph>& morphData, const float weight) const {
	for (const auto& [m_boneIndex, m_position, m_quaternion] : morphData) {
		auto* node = nodes[m_boneIndex].get();
		node->translate += m_position * weight;
		const glm::quat q = glm::slerp(glm::quat(1,0,0,0), m_quaternion, weight);
		node->rotate = glm::normalize(q * node->rotate);
	}
}

