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
	for (const auto& node : m_nodes) {
		node->m_animTranslate = glm::vec3(0);
		node->m_animRotate = glm::quat(1, 0, 0, 0);
	}
	BeginAnimation();
	for (const auto& morph : m_morphs)
		morph->m_weight = 0;
	for (const auto& ikSolver : m_ikSolvers)
		ikSolver->m_enable = true;
	UpdateNodeAnimation(false);
	UpdateNodeAnimation(true);
	ResetPhysics();
}

void Model::SaveBaseAnimation() const {
	for (const auto& node : m_nodes) {
		node->m_baseAnimTranslate = node->m_animTranslate;
		node->m_baseAnimRotate = node->m_animRotate;
	}
	for (const auto& morph : m_morphs)
		morph->m_saveAnimWeight = morph->m_weight;
	for (const auto& ikSolver : m_ikSolvers)
		ikSolver->m_baseAnimEnable = ikSolver->m_enable;
}

void Model::ClearBaseAnimation() const {
	for (const auto& node : m_nodes) {
		node->m_baseAnimTranslate = glm::vec3(0);
		node->m_baseAnimRotate = glm::quat(1, 0, 0, 0);
	}
	for (const auto& morph : m_morphs)
		morph->m_saveAnimWeight = 0;
	for (const auto& ikSolver : m_ikSolvers)
		ikSolver->m_baseAnimEnable = true;
}

void Model::BeginAnimation() {
	for (const auto& node : m_nodes)
		node->BeginUpdateTransform();
	std::ranges::fill(m_morphPositions, glm::vec3(0));
	std::ranges::fill(m_morphUVs, glm::vec4(0));
}

void Model::UpdateMorphAnimation() {
	BeginMorphMaterial();
	for (const auto& morph : m_morphs)
		EvalMorph(morph.get(), morph->m_weight);
	EndMorphMaterial();
}

void Model::UpdateNodeAnimation(const bool afterPhysicsAnim) const {
	const auto pred = [&](const Node* node) { return node->m_isDeformAfterPhysics == afterPhysicsAnim; };
	for (auto* node : m_sortedNodes | std::views::filter(pred))
		node->UpdateLocalTransform();
	for (auto* node : m_sortedNodes | std::views::filter(pred)) {
		if (!node->m_parent)
			node->UpdateGlobalTransform();
	}
	for (auto* node : m_sortedNodes | std::views::filter(pred)) {
		if (node->m_appendNode) {
			node->UpdateAppendTransform();
			node->UpdateGlobalTransform();
		}
		if (node->m_ikSolver) {
			node->m_ikSolver->Solve();
			node->UpdateGlobalTransform();
		}
	}
}

void Model::ResetPhysics() const {
	for (auto& rb : m_rigidBodies) {
		rb->SetActivation(false);
		rb->ResetTransform();
	}
	m_physics->m_world->stepSimulation(1.0f / 60.0f, m_physics->m_maxSubStepCount, 1.0 / m_physics->m_fps);
	for (auto& rb : m_rigidBodies) {
		rb->ReflectGlobalTransform();
		rb->CalcLocalTransform();
	}
	for (const auto& node : m_nodes) {
		if (!node->m_parent)
			node->UpdateGlobalTransform();
	}
	for (auto& rb : m_rigidBodies)
		rb->Reset(m_physics.get());
}

void Model::UpdatePhysicsAnimation(const float elapsed) const {
	for (auto& rb : m_rigidBodies)
		rb->SetActivation(true);
	m_physics->m_world->stepSimulation(elapsed, m_physics->m_maxSubStepCount, 1.0 / m_physics->m_fps);
	for (auto& rb : m_rigidBodies) {
		rb->ReflectGlobalTransform();
		rb->CalcLocalTransform();
	}
	for (const auto& node : m_nodes) {
		if (!node->m_parent)
			node->UpdateGlobalTransform();
	}
}

void Model::Update() {
	for (size_t i = 0; i < m_nodes.size(); i++)
		m_transforms[i] = m_nodes[i]->m_global * m_nodes[i]->m_inverseInit;
	if (m_parallelUpdateCount != m_updateRanges.size())
		SetupParallelUpdate();
	const size_t futureCount = m_parallelUpdateFutures.size();
	for (size_t i = 0; i < futureCount; i++) {
		if (m_updateRanges[i + 1].m_vertexCount != 0) {
			m_parallelUpdateFutures[i] = std::async(std::launch::async,
			[this, range = m_updateRanges[i + 1]] { this->Update(range); }
			);
		}
	}
	Update(m_updateRanges[0]);
	for (size_t i = 0; i < futureCount; i++) {
		if (m_updateRanges[i + 1].m_vertexCount != 0)
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
	std::filesystem::path dirPath = filepath.parent_path();
	size_t vertexCount = pmx.m_vertices.size();
	m_positions.reserve(vertexCount);
	m_normals.reserve(vertexCount);
	m_uvs.reserve(vertexCount);
	m_vertexBoneInfos.reserve(vertexCount);
	m_bboxMax = glm::vec3(-std::numeric_limits<float>::max());
	m_bboxMin = glm::vec3(std::numeric_limits<float>::max());
	for (const auto& v : pmx.m_vertices) {
		glm::vec3 pos = v.m_position * glm::vec3(1, 1, -1);
		m_positions.push_back(pos);
		m_normals.push_back(v.m_normal * glm::vec3(1, 1, -1));
		m_uvs.push_back(glm::vec2(v.m_uv.x, 1.0f - v.m_uv.y));
		Vertex vtxBoneInfo{};
		if (WeightType::SDEF != v.m_weightType) {
			vtxBoneInfo.m_boneIndices[0] = v.m_boneIndices[0];
			vtxBoneInfo.m_boneIndices[1] = v.m_boneIndices[1];
			vtxBoneInfo.m_boneIndices[2] = v.m_boneIndices[2];
			vtxBoneInfo.m_boneIndices[3] = v.m_boneIndices[3];
			vtxBoneInfo.m_boneWeights[0] = v.m_boneWeights[0];
			vtxBoneInfo.m_boneWeights[1] = v.m_boneWeights[1];
			vtxBoneInfo.m_boneWeights[2] = v.m_boneWeights[2];
			vtxBoneInfo.m_boneWeights[3] = v.m_boneWeights[3];
		}
		vtxBoneInfo.m_weightType = v.m_weightType;
		switch (v.m_weightType) {
			case WeightType::BDEF2:
				vtxBoneInfo.m_boneWeights[1] = 1.0f - vtxBoneInfo.m_boneWeights[0];
				break;
			case WeightType::SDEF: {
					auto w0 = v.m_boneWeights[0];
					auto w1 = 1.0f - w0;
					auto center = v.m_sdefC * glm::vec3(1, 1, -1);
					auto r0 = v.m_sdefR0 * glm::vec3(1, 1, -1);
					auto r1 = v.m_sdefR1 * glm::vec3(1, 1, -1);
					auto rw = r0 * w0 + r1 * w1;
					r0 = center + r0 - rw;
					r1 = center + r1 - rw;
					auto cr0 = (center + r0) * 0.5f;
					auto cr1 = (center + r1) * 0.5f;
					vtxBoneInfo.m_boneIndices[0] = v.m_boneIndices[0];
					vtxBoneInfo.m_boneIndices[1] = v.m_boneIndices[1];
					vtxBoneInfo.m_boneWeights[0] = v.m_boneWeights[0];
					vtxBoneInfo.m_sdefC = center;
					vtxBoneInfo.m_sdefR0 = cr0;
					vtxBoneInfo.m_sdefR1 = cr1;
				}
				break;
			default:
				break;
		}
		m_vertexBoneInfos.push_back(vtxBoneInfo);
		m_bboxMax = glm::max(m_bboxMax, pos);
		m_bboxMin = glm::min(m_bboxMin, pos);
	}
	m_morphPositions.resize(m_positions.size());
	m_morphUVs.resize(m_positions.size());
	m_updatePositions.resize(m_positions.size());
	m_updateNormals.resize(m_normals.size());
	m_updateUVs.resize(m_uvs.size());
	m_indexElementSize = pmx.m_header.m_vertexIndexSize;
	m_indices.resize(pmx.m_faces.size() * 3 * m_indexElementSize);
	m_indexCount = pmx.m_faces.size() * 3;
	auto fillIndices = [&](auto* out) {
		int idx = 0;
		for (const auto& [tri] : pmx.m_faces) {
			out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[2]);
			out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[1]);
			out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[0]);
		}
	};
	switch (m_indexElementSize) {
		case 1: fillIndices(reinterpret_cast<uint8_t*>(m_indices.data())); break;
		case 2: fillIndices(reinterpret_cast<uint16_t*>(m_indices.data())); break;
		case 4: fillIndices(reinterpret_cast<uint32_t*>(m_indices.data())); break;
		default: return false;
	}
	std::vector<std::filesystem::path> texturePaths;
	texturePaths.reserve(pmx.m_textures.size());
	for (const auto& [m_textureName] : pmx.m_textures) {
		std::filesystem::path texPath = dirPath / m_textureName;
		texturePaths.emplace_back(std::move(texPath));
	}
	m_materials.reserve(pmx.m_materials.size());
	m_subMeshes.reserve(pmx.m_materials.size());
	uint32_t beginIndex = 0;
	for (const auto& mat : pmx.m_materials) {
		const auto dm = static_cast<uint8_t>(mat.m_drawMode);
		Material m;
		m.m_diffuse = mat.m_diffuse;
		m.m_specularPower = mat.m_specularPower;
		m.m_specular = mat.m_specular;
		m.m_ambient = mat.m_ambient;
		m.m_spTextureMode = SphereMode::None;
		m.m_bothFace      = (dm & static_cast<uint8_t>(DrawModeFlags::BothFace)) != 0;
		m.m_edgeFlag      = (dm & static_cast<uint8_t>(DrawModeFlags::DrawEdge)) != 0 ? 1 : 0;
		m.m_groundShadow  = (dm & static_cast<uint8_t>(DrawModeFlags::GroundShadow)) != 0;
		m.m_shadowCaster  = (dm & static_cast<uint8_t>(DrawModeFlags::CastSelfShadow)) != 0;
		m.m_shadowReceiver= (dm & static_cast<uint8_t>(DrawModeFlags::ReceiveSelfShadow)) != 0;
		m.m_edgeSize = mat.m_edgeSize;
		m.m_edgeColor = mat.m_edgeColor;
		if (mat.m_textureIndex != -1)
			m.m_texture = texturePaths[mat.m_textureIndex];
		if (mat.m_toonMode == ToonMode::Common) {
			if (mat.m_toonTextureIndex != -1) {
				std::stringstream ss;
				ss << "toon" << std::setfill('0') << std::setw(2) << (mat.m_toonTextureIndex + 1) << ".bmp";
				m.m_toonTexture = dataDir / ss.str();
			}
		} else if (mat.m_toonMode == ToonMode::Separate) {
			if (mat.m_toonTextureIndex != -1)
				m.m_toonTexture = texturePaths[mat.m_toonTextureIndex];
		}
		if (mat.m_sphereTextureIndex != -1) {
			m.m_spTexture = texturePaths[mat.m_sphereTextureIndex];
			m.m_spTextureMode = mat.m_sphereMode;
		}
		m_materials.emplace_back(std::move(m));
		SubMesh subMesh{};
		subMesh.m_beginIndex = static_cast<int>(beginIndex);
		subMesh.m_vertexCount = mat.m_numFaceVertices;
		subMesh.m_materialID = static_cast<int>(m_materials.size() - 1);
		m_subMeshes.push_back(subMesh);
		beginIndex += mat.m_numFaceVertices;
	}
	m_initMaterials = m_materials;
	m_mulMaterialFactors.resize(m_materials.size());
	m_addMaterialFactors.resize(m_materials.size());
	m_nodes.reserve(pmx.m_bones.size());
	for (const auto& bone : pmx.m_bones) {
		auto node = std::make_unique<Node>();
		node->m_index = static_cast<uint32_t>(m_nodes.size());
		node->m_name = bone.m_name;
		m_nodes.emplace_back(std::move(node));
	}
	for (size_t i = 0; i < pmx.m_bones.size(); i++) {
		const auto& bone = pmx.m_bones[i];
		auto* node = m_nodes[i].get();
		glm::vec3 localPos = bone.m_position;
		if (bone.m_parentBoneIndex != -1) {
			auto* parent = m_nodes[bone.m_parentBoneIndex].get();
			parent->AddChild(node);
			localPos -= pmx.m_bones[bone.m_parentBoneIndex].m_position;
		}
		localPos.z *= -1;
		node->m_translate = localPos;
		node->m_global = glm::translate(glm::mat4(1), bone.m_position * glm::vec3(1, 1, -1));
		node->m_inverseInit = glm::inverse(node->m_global);
		node->m_deformDepth = bone.m_deformDepth;
		bool deformAfterPhysics = !!(static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::DeformAfterPhysics));
		node->m_isDeformAfterPhysics = deformAfterPhysics;
		bool appendRotate = (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::AppendRotate)) != 0;
		bool appendTranslate = (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::AppendTranslate)) != 0;
		node->m_isAppendRotate = appendRotate;
		node->m_isAppendTranslate = appendTranslate;
		if ((appendRotate || appendTranslate) && bone.m_appendBoneIndex != -1) {
			bool appendLocal = (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::AppendLocal)) != 0;
			auto* appendNode = m_nodes[bone.m_appendBoneIndex].get();
			float appendWeight = bone.m_appendWeight;
			node->m_isAppendLocal = appendLocal;
			node->m_appendNode = appendNode;
			node->m_appendWeight = appendWeight;
		}
		node->m_initTranslate = node->m_translate;
		node->m_initRotate = node->m_rotate;
		node->m_initScale = node->m_scale;
	}
	m_transforms.resize(m_nodes.size());
	m_sortedNodes.clear();
	m_sortedNodes.reserve(m_nodes.size());
	for (auto& node : m_nodes)
		m_sortedNodes.push_back(node.get());
	std::ranges::stable_sort(m_sortedNodes,
		[](const Node* x, const Node* y) { return x->m_deformDepth < y->m_deformDepth; }
	);
	for (size_t i = 0; i < pmx.m_bones.size(); i++) {
		const auto& bone = pmx.m_bones[i];
		if (static_cast<uint16_t>(bone.m_boneFlag) & static_cast<uint16_t>(BoneFlags::IK)) {
			auto solver = std::make_unique<IkSolver>();
			solver->m_ikNode = m_nodes[i].get();
			m_nodes[i]->m_ikSolver = solver.get();
			solver->m_ikTarget = m_nodes[bone.m_ikTargetBoneIndex].get();
			for (const auto& [m_ikBoneIndex, m_enableLimit, m_limitMin, m_limitMax] : bone.m_ikLinks) {
				auto* linkNode = m_nodes[m_ikBoneIndex].get();
				IKChain chain{};
				chain.m_node = linkNode;
				chain.m_enableAxisLimit = m_enableLimit;
				chain.m_limitMin = m_limitMax * glm::vec3(-1);
				chain.m_limitMax = m_limitMin * glm::vec3(-1);
				chain.m_saveIKRot = glm::quat(1, 0, 0, 0);
				solver->m_chains.emplace_back(chain);
				linkNode->m_enableIK = true;
			}
			solver->m_iterateCount = bone.m_ikIterationCount;
			solver->m_limitAngle = bone.m_ikLimit;
			m_ikSolvers.emplace_back(std::move(solver));
		}
	}
	for (const auto& morph : pmx.m_morphs) {
		auto m = std::make_unique<Morph>();
		m->m_name = morph.m_name;
		m->m_weight = 0.0f;
		m->m_morphType = morph.m_morphType;
		if (morph.m_morphType == MorphType::Position) {
			m->m_dataIndex = m_positionMorphDatas.size();
			std::vector<PositionMorph> morphData;
			for (const auto& [m_vertexIndex, m_position] : morph.m_positionMorph) {
				PositionMorph morphVtx{};
				morphVtx.m_vertexIndex = m_vertexIndex;
				morphVtx.m_position = m_position * glm::vec3(1, 1, -1);
				morphData.push_back(morphVtx);
			}
			m_positionMorphDatas.emplace_back(std::move(morphData));
		} else if (morph.m_morphType == MorphType::UV) {
			m->m_dataIndex = m_uvMorphDatas.size();
			std::vector<UVMorph> morphData;
			for (const auto& [m_vertexIndex, m_uv] : morph.m_uvMorph) {
				UVMorph morphUV{};
				morphUV.m_vertexIndex = m_vertexIndex;
				morphUV.m_uv = m_uv;
				morphData.push_back(morphUV);
			}
			m_uvMorphDatas.emplace_back(std::move(morphData));
		} else if (morph.m_morphType == MorphType::Material) {
			m->m_dataIndex = m_materialMorphDatas.size();
			m_materialMorphDatas.emplace_back(morph.m_materialMorph);
		} else if (morph.m_morphType == MorphType::Bone) {
			m->m_dataIndex = m_boneMorphDatas.size();
			std::vector<BoneMorph> boneMorphData;
			for (const auto& [m_boneIndex, m_position, m_quaternion] : morph.m_boneMorph) {
				BoneMorph boneMorphElem{};
				boneMorphElem.m_boneIndex = m_boneIndex;
				boneMorphElem.m_position = m_position * glm::vec3(1, 1, -1);
				auto rot = Util::InvZ(glm::mat3_cast(m_quaternion));
				boneMorphElem.m_quaternion = glm::quat_cast(rot);
				boneMorphData.push_back(boneMorphElem);
			}
			m_boneMorphDatas.emplace_back(boneMorphData);
		} else if (morph.m_morphType == MorphType::Group) {
			m->m_dataIndex = m_groupMorphDatas.size();
			m_groupMorphDatas.emplace_back(morph.m_groupMorph);
		}
		m_morphs.emplace_back(std::move(m));
	}
	std::vector<int32_t> groupMorphStack;
	std::function<void(int32_t)> fixInfiniteGroupMorph;
	fixInfiniteGroupMorph = [&](const int32_t idx) {
		const auto* morph = m_morphs[idx].get();
		if (morph->m_morphType != MorphType::Group)
			return;
		for (auto& [childIdx, w] : m_groupMorphDatas[morph->m_dataIndex]) {
			if (childIdx < 0)
				continue;
			if (std::ranges::find(groupMorphStack, childIdx) != groupMorphStack.end()) {
				childIdx = -1;
				continue;
			}
			groupMorphStack.push_back(idx);
			fixInfiniteGroupMorph(childIdx);
			groupMorphStack.pop_back();
		}
	};
	for (int32_t i = 0; i < static_cast<int32_t>(m_morphs.size()); i++) {
		fixInfiniteGroupMorph(i);
		groupMorphStack.clear();
	}
	m_physics = std::make_unique<Physics>();
	m_physics->Create();
	for (const auto& rigidBody : pmx.m_rigidBodies) {
		auto rb = std::make_unique<RigidBody>();
		Node* node = nullptr;
		if (rigidBody.m_boneIndex != -1)
			node = m_nodes[rigidBody.m_boneIndex].get();
		rb->Create(rigidBody, this, node);
		m_physics->m_world->addRigidBody(rb->m_rigidBody.get(), 1 << rb->m_group, rb->m_groupMask);
		m_rigidBodies.emplace_back(std::move(rb));
	}
	for (const auto& joint : pmx.m_joints) {
		if (joint.m_rigidbodyAIndex != -1 &&
		    joint.m_rigidbodyBIndex != -1 &&
		    joint.m_rigidbodyAIndex != joint.m_rigidbodyBIndex) {
			auto j = std::make_unique<Joint>();
			j->CreateJoint(joint,
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
	m_materials.clear();
	m_subMeshes.clear();
	m_positions.clear();
	m_normals.clear();
	m_uvs.clear();
	m_vertexBoneInfos.clear();
	m_indices.clear();
	m_nodes.clear();
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
	if (m_parallelUpdateCount == 0)
		m_parallelUpdateCount = std::thread::hardware_concurrency();
	const size_t maxParallelCount = std::max(static_cast<size_t>(16),
		static_cast<size_t>(std::thread::hardware_concurrency()));
	if (m_parallelUpdateCount > maxParallelCount)
		m_parallelUpdateCount = 16;

	m_updateRanges.resize(m_parallelUpdateCount);
	m_parallelUpdateFutures.resize(m_parallelUpdateCount - 1);

	const size_t vertexCount = m_positions.size();
	constexpr size_t LowerVertexCount = 1000;
	if (vertexCount < m_updateRanges.size() * LowerVertexCount) {
		const size_t numRanges = (vertexCount + LowerVertexCount - 1) / LowerVertexCount;
		for (size_t rangeIdx = 0; rangeIdx < m_updateRanges.size(); rangeIdx++) {
			auto& [m_vertexOffset, m_vertexCount] = m_updateRanges[rangeIdx];
			if (rangeIdx < numRanges) {
				m_vertexOffset = rangeIdx * LowerVertexCount;
				m_vertexCount = std::min(LowerVertexCount, vertexCount - m_vertexOffset);
			} else {
				m_vertexOffset = 0;
				m_vertexCount = 0;
			}
		}
	} else {
		const size_t numVertexCount = vertexCount / m_updateRanges.size();
		size_t offset = 0;
		for (size_t rangeIdx = 0; rangeIdx < m_updateRanges.size(); rangeIdx++) {
			auto& [m_vertexOffset, m_vertexCount] = m_updateRanges[rangeIdx];
			m_vertexOffset = offset;
			m_vertexCount = numVertexCount;
			if (rangeIdx == 0)
				m_vertexCount += vertexCount % m_updateRanges.size();
			offset = m_vertexOffset + m_vertexCount;
		}
	}
}

void Model::Update(const UpdateRange& range) {
	const auto* position = m_positions.data() + range.m_vertexOffset;
	const auto* normal = m_normals.data() + range.m_vertexOffset;
	const auto* uv = m_uvs.data() + range.m_vertexOffset;
	const auto* morphPos = m_morphPositions.data() + range.m_vertexOffset;
	const auto* morphUV = m_morphUVs.data() + range.m_vertexOffset;
	const auto* vtxInfo = m_vertexBoneInfos.data() + range.m_vertexOffset;
	const auto* transforms = m_transforms.data();
	auto* updatePosition = m_updatePositions.data() + range.m_vertexOffset;
	auto* updateNormal = m_updateNormals.data() + range.m_vertexOffset;
	auto* updateUV = m_updateUVs.data() + range.m_vertexOffset;

	for (size_t i = 0; i < range.m_vertexCount; i++) {
		glm::mat4 m;
		switch (vtxInfo->m_weightType) {
			case WeightType::BDEF1: {
				const auto i0 = vtxInfo->m_boneIndices[0];
				const auto& m0 = transforms[i0];
				m = m0;
				break;
			}
			case WeightType::BDEF2: {
				const auto i0 = vtxInfo->m_boneIndices[0];
				const auto i1 = vtxInfo->m_boneIndices[1];
				const auto w0 = vtxInfo->m_boneWeights[0];
				const auto w1 = vtxInfo->m_boneWeights[1];
				const auto& m0 = transforms[i0];
				const auto& m1 = transforms[i1];
				m = m0 * w0 + m1 * w1;
				break;
			}
			case WeightType::BDEF4: {
				const auto i0 = vtxInfo->m_boneIndices[0];
				const auto i1 = vtxInfo->m_boneIndices[1];
				const auto i2 = vtxInfo->m_boneIndices[2];
				const auto i3 = vtxInfo->m_boneIndices[3];
				const auto w0 = vtxInfo->m_boneWeights[0];
				const auto w1 = vtxInfo->m_boneWeights[1];
				const auto w2 = vtxInfo->m_boneWeights[2];
				const auto w3 = vtxInfo->m_boneWeights[3];
				const auto& m0 = transforms[i0];
				const auto& m1 = transforms[i1];
				const auto& m2 = transforms[i2];
				const auto& m3 = transforms[i3];
				m = m0 * w0 + m1 * w1 + m2 * w2 + m3 * w3;
				break;
			}
			case WeightType::SDEF: {
				const auto i0 = vtxInfo->m_boneIndices[0];
				const auto i1 = vtxInfo->m_boneIndices[1];
				const auto w0 = vtxInfo->m_boneWeights[0];
				const auto w1 = 1.0f - w0;
				const auto center = vtxInfo->m_sdefC;
				const auto cr0 = vtxInfo->m_sdefR0;
				const auto cr1 = vtxInfo->m_sdefR1;
				const auto q0 = glm::quat_cast(m_nodes[i0]->m_global);
				const auto q1 = glm::quat_cast(m_nodes[i1]->m_global);
				const auto m0 = transforms[i0];
				const auto m1 = transforms[i1];

				const auto pos = *position + *morphPos;
				const auto rot_mat = glm::mat3_cast(glm::slerp(q0, q1, w1));

				*updatePosition = rot_mat * (pos - center) + glm::vec3(m0 * glm::vec4(cr0, 1)) * w0 +
					glm::vec3(m1 * glm::vec4(cr1, 1)) * w1;
				*updateNormal = rot_mat * *normal;

				break;
			}
			case WeightType::QDEF: {
				glm::dualquat dq[4];
				float w[4] = {};
				for (int bi = 0; bi < 4; bi++) {
					auto boneID = vtxInfo->m_boneIndices[bi];
					if (boneID != -1) {
						dq[bi] = glm::dualquat_cast(glm::mat3x4(glm::transpose(transforms[boneID])));
						dq[bi] = glm::normalize(dq[bi]);
						w[bi] = vtxInfo->m_boneWeights[bi];
					} else
						w[bi] = 0;
				}
				if (glm::dot(dq[0].real, dq[1].real) < 0) w[1] *= -1.0f;
				if (glm::dot(dq[0].real, dq[2].real) < 0) w[2] *= -1.0f;
				if (glm::dot(dq[0].real, dq[3].real) < 0) w[3] *= -1.0f;
				auto blendDQ = w[0] * dq[0] + w[1] * dq[1] + w[2] * dq[2] + w[3] * dq[3];
				blendDQ = glm::normalize(blendDQ);
				m = glm::transpose(glm::mat3x4_cast(blendDQ));
				break;
			}
			default:
				break;
		}

		if (WeightType::SDEF != vtxInfo->m_weightType) {
			*updatePosition = glm::vec3(m * glm::vec4(*position + *morphPos, 1));
			*updateNormal = glm::normalize(glm::mat3(m) * *normal);
		}
		*updateUV = *uv + glm::vec2(morphUV->x, morphUV->y);

		vtxInfo++;
		position++;
		normal++;
		uv++;
		updatePosition++;
		updateNormal++;
		updateUV++;
		morphPos++;
		morphUV++;
	}
}

void Model::EvalMorph(const Morph* morph, const float weight) {
	switch (morph->m_morphType) {
		case MorphType::Position:
			MorphPosition(m_positionMorphDatas[morph->m_dataIndex], weight);
			break;
		case MorphType::UV:
			MorphUV(m_uvMorphDatas[morph->m_dataIndex], weight);
			break;
		case MorphType::Material:
			MorphMaterial(m_materialMorphDatas[morph->m_dataIndex], weight);
			break;
		case MorphType::Bone:
			MorphBone(m_boneMorphDatas[morph->m_dataIndex], weight);
			break;
		case MorphType::Group: {
			for (const auto& [m_morphIndex, m_weight] : m_groupMorphDatas[morph->m_dataIndex]) {
				if (m_morphIndex == -1)
					continue;
				auto& elemMorph = m_morphs[m_morphIndex];
				EvalMorph(elemMorph.get(), m_weight * weight);
			}
			break;
		}
		default:
			break;
	}
}

void Model::MorphPosition(const std::vector<PositionMorph>& morphData, const float weight) {
	if (weight == 0)
		return;

	for (const auto& [m_index, m_position] : morphData)
		m_morphPositions[m_index] += m_position * weight;
}

void Model::MorphUV(const std::vector<UVMorph>& morphData, const float weight) {
	if (weight == 0)
		return;

	for (const auto& [m_index, m_uv] : morphData)
		m_morphUVs[m_index] += m_uv * weight;
}

void Model::BeginMorphMaterial() {
	MaterialMorph initMul{};
	initMul.m_diffuse = glm::vec4(1);
	initMul.m_specular = glm::vec3(1);
	initMul.m_specularPower = 1;
	initMul.m_ambient = glm::vec3(1);
	initMul.m_edgeColor = glm::vec4(1);
	initMul.m_edgeSize = 1;
	initMul.m_textureFactor = glm::vec4(1);
	initMul.m_sphereTextureFactor = glm::vec4(1);
	initMul.m_toonTextureFactor = glm::vec4(1);

	MaterialMorph initAdd{};
	initAdd.m_diffuse = glm::vec4(0);
	initAdd.m_specular = glm::vec3(0);
	initAdd.m_specularPower = 0;
	initAdd.m_ambient = glm::vec3(0);
	initAdd.m_edgeColor = glm::vec4(0);
	initAdd.m_edgeSize = 0;
	initAdd.m_textureFactor = glm::vec4(0);
	initAdd.m_sphereTextureFactor = glm::vec4(0);
	initAdd.m_toonTextureFactor = glm::vec4(0);

	const size_t matCount = m_materials.size();
	for (size_t matIdx = 0; matIdx < matCount; matIdx++) {
		m_mulMaterialFactors[matIdx] = initMul;
		m_mulMaterialFactors[matIdx].m_diffuse = m_initMaterials[matIdx].m_diffuse;
		m_mulMaterialFactors[matIdx].m_specular = m_initMaterials[matIdx].m_specular;
		m_mulMaterialFactors[matIdx].m_specularPower = m_initMaterials[matIdx].m_specularPower;
		m_mulMaterialFactors[matIdx].m_ambient = m_initMaterials[matIdx].m_ambient;

		m_addMaterialFactors[matIdx] = initAdd;
	}
}

void Model::EndMorphMaterial() {
	const size_t matCount = m_materials.size();
	for (size_t matIdx = 0; matIdx < matCount; matIdx++) {
		MaterialMorph matFactor = m_mulMaterialFactors[matIdx];
		Add(matFactor, m_addMaterialFactors[matIdx], 1.0f);

		m_materials[matIdx].m_diffuse = matFactor.m_diffuse;
		m_materials[matIdx].m_specular = matFactor.m_specular;
		m_materials[matIdx].m_specularPower = matFactor.m_specularPower;
		m_materials[matIdx].m_ambient = matFactor.m_ambient;
		m_materials[matIdx].m_textureMulFactor = m_mulMaterialFactors[matIdx].m_textureFactor;
		m_materials[matIdx].m_textureAddFactor = m_addMaterialFactors[matIdx].m_textureFactor;
		m_materials[matIdx].m_spTextureMulFactor = m_mulMaterialFactors[matIdx].m_sphereTextureFactor;
		m_materials[matIdx].m_spTextureAddFactor = m_addMaterialFactors[matIdx].m_sphereTextureFactor;
		m_materials[matIdx].m_toonTextureMulFactor = m_mulMaterialFactors[matIdx].m_toonTextureFactor;
		m_materials[matIdx].m_toonTextureAddFactor = m_addMaterialFactors[matIdx].m_toonTextureFactor;
	}
}

void Model::MorphMaterial(const std::vector<MaterialMorph>& morphData, const float weight) {
	for (const auto& matMorph : morphData) {
		if (matMorph.m_materialIndex != -1) {
			const auto mi = matMorph.m_materialIndex;
			switch (matMorph.m_opType) {
				case OpType::Mul:
					Mul(m_mulMaterialFactors[mi], matMorph, weight);
					break;
				case OpType::Add:
					Add(m_addMaterialFactors[mi], matMorph, weight);
					break;
				default:
					break;
			}
		} else {
			switch (matMorph.m_opType) {
				case OpType::Mul:
					for (size_t i = 0; i < m_materials.size(); i++)
						Mul(m_mulMaterialFactors[i], matMorph, weight);
					break;
				case OpType::Add:
					for (size_t i = 0; i < m_materials.size(); i++)
						Add(m_addMaterialFactors[i], matMorph, weight);
					break;
				default:
					break;
			}
		}
	}
}

void Model::MorphBone(const std::vector<BoneMorph>& morphData, const float weight) const {
	for (const auto& [m_boneIndex, m_position, m_quaternion] : morphData) {
		auto* node = m_nodes[m_boneIndex].get();
		glm::vec3 t = glm::mix(glm::vec3(0), m_position, weight);
		node->m_translate = node->m_translate + t;
		const glm::quat q = glm::slerp(node->m_rotate, m_quaternion, weight);
		node->m_rotate = q;
	}
}
