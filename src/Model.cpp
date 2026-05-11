#include "Model.h"

#include "animation/ModelAnimation.h"
#include "Util.h"

#include <ranges>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>

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
	std::ranges::fill(morphPositions, glm::vec3(0));
	std::ranges::fill(morphUVs, glm::vec4(0));
}

void Model::UpdateMorphAnimation() {
	BeginMorphMaterial();
	for (const auto& morph : morphs)
		EvalMorph(morph.get(), morph->weight);
	EndMorphMaterial();
}

void Model::UpdateNodeAnimation(const bool afterPhysicsAnim) const {
	const auto Pred = [&](const std::reference_wrapper<Node>& node) {
		return node.get().isDeformAfterPhysics == afterPhysicsAnim;
	};
	for (auto& nodeRef : sortedNodes | std::views::filter(Pred))
		nodeRef.get().UpdateLocalTransform();
	for (auto& nodeRef : sortedNodes | std::views::filter(Pred)) {
		auto& node = nodeRef.get();
		if (node.parent.expired())
			node.UpdateGlobalTransform();
	}
	for (auto& nodeRef : sortedNodes | std::views::filter(Pred)) {
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
	for (auto& rb : rigidBodies) {
		rb->ApplyActivation(false);
		rb->ResetTransform();
	}
	physics->world->stepSimulation(
		1.0f / 60.0f, physics->maxSubStepCount,
		static_cast<btScalar>(1.0f / physics->fps));
	for (auto& rb : rigidBodies) {
		rb->ReflectGlobalTransform();
		rb->CalcLocalTransform();
	}
	for (const auto& node : nodes) {
		if (node->parent.expired())
			node->UpdateGlobalTransform();
	}
	for (auto& rb : rigidBodies)
		rb->Reset(physics.get());
}

void Model::UpdatePhysicsAnimation(const float elapsed) const {
	for (auto& rb : rigidBodies)
		rb->ApplyActivation(true);
	physics->world->stepSimulation(
		elapsed, physics->maxSubStepCount,
		static_cast<btScalar>(1.0f / physics->fps));
	for (auto& rb : rigidBodies) {
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
		transforms[i] = nodes[i]->global * nodes[i]->inverseInit;
	if (parallelUpdateCount != updateRanges.size())
		SetupParallelUpdate();
	const size_t futureCount = parallelUpdateFutures.size();
	for (size_t i = 0; i < futureCount; i++) {
		if (updateRanges[i + 1].vertexCount != 0) {
			parallelUpdateFutures[i] = std::async(std::launch::async,
			[this, range = updateRanges[i + 1]] { this->Update(range); }
			);
		}
	}
	Update(updateRanges[0]);
	for (size_t i = 0; i < futureCount; i++) {
		if (updateRanges[i + 1].vertexCount != 0)
			parallelUpdateFutures[i].wait();
	}
}

void Model::UpdateAllAnimation(const ModelAnimation* anim, const float frame, const float physicsElapsed) {
	if (anim)
		anim->Evaluate(frame);
	UpdateMorphAnimation();
	UpdateNodeAnimation(false);
	UpdatePhysicsAnimation(physicsElapsed);
	UpdateNodeAnimation(true);
}

bool Model::Load(const std::filesystem::path& filepath, const std::filesystem::path& dataDir) {
	Destroy();
	PmxReader pmx;
	if (!pmx.ReadFile(filepath))
		return false;
	modelName        = pmx.info.modelName;
	englishModelName = pmx.info.englishModelName;
	comment          = pmx.info.comment;
	englishComment   = pmx.info.englishComment;
	std::filesystem::path dirPath = filepath.parent_path();
	size_t vertexCount = pmx.vertices.size();
	positions.reserve(vertexCount);
	normals.reserve(vertexCount);
	uvs.reserve(vertexCount);
	vertexBoneInfos.reserve(vertexCount);
	bboxMax = glm::vec3(-std::numeric_limits<float>::max)();
	bboxMin = glm::vec3(std::numeric_limits<float>::max)();
	constexpr glm::vec3 invZ(1, 1, -1);
	for (const auto& v : pmx.vertices) {
		glm::vec3 pos = v.position * invZ;
		positions.push_back(pos);
		normals.push_back(v.normal * invZ);
		uvs.emplace_back(v.uv.x, 1.0f - v.uv.y);
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
		vertexBoneInfos.push_back(vtxBoneInfo);
		bboxMax = (glm::max)(bboxMax, pos);
		bboxMin = (glm::min)(bboxMin, pos);
	}
	morphPositions.resize(positions.size());
	morphUVs.resize(positions.size());
	updatePositions.resize(positions.size());
	updateNormals.resize(normals.size());
	updateUVs.resize(uvs.size());
	indexElementSize = pmx.header.vertexIndexSize;
	indices.resize(pmx.faces.size() * 3 * indexElementSize);
	indexCount = pmx.faces.size() * 3;
	auto FillIndices = [&](auto* out) {
		int idx = 0;
		for (const auto& [tri] : pmx.faces) {
			out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[2]);
			out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[1]);
			out[idx++] = static_cast<std::remove_pointer_t<decltype(out)>>(tri[0]);
		}
	};
	switch (indexElementSize) {
		case 1: FillIndices(reinterpret_cast<uint8_t*>(indices.data())); break;
		case 2: FillIndices(reinterpret_cast<uint16_t*>(indices.data())); break;
		case 4: FillIndices(reinterpret_cast<uint32_t*>(indices.data())); break;
		default: return false;
	}
	std::vector<std::filesystem::path> texturePaths;
	texturePaths.reserve(pmx.textures.size());
	for (const auto& [textureName] : pmx.textures) {
		std::filesystem::path texPath = dirPath / textureName;
		texturePaths.emplace_back(std::move(texPath));
	}
	materials.reserve(pmx.materials.size());
	subMeshes.reserve(pmx.materials.size());
	uint32_t beginIndex = 0;
	for (const auto& mat : pmx.materials) {
		const auto dm = static_cast<uint8_t>(mat.drawMode);
		Material m;
		m.diffuse = mat.diffuse;
		m.specularPower = mat.specularPower;
		m.specular = mat.specular;
		m.ambient = mat.ambient;
		m.spTextureMode = SphereMode::None;
		m.bothFace      = (dm & static_cast<uint8_t>(DrawModeFlags::BothFace)) != 0;
		m.edgeFlag      = (dm & static_cast<uint8_t>(DrawModeFlags::DrawEdge)) != 0 ? 1 : 0;
		m.groundShadow  = (dm & static_cast<uint8_t>(DrawModeFlags::GroundShadow)) != 0;
		m.shadowCaster  = (dm & static_cast<uint8_t>(DrawModeFlags::CastSelfShadow)) != 0;
		m.shadowReceiver= (dm & static_cast<uint8_t>(DrawModeFlags::ReceiveSelfShadow)) != 0;
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
		materials.emplace_back(std::move(m));
		SubMesh subMesh{};
		subMesh.beginIndex = static_cast<int>(beginIndex);
		subMesh.indexCount = mat.numFaceVertices;
		subMesh.materialId = static_cast<int>(materials.size() - 1);
		subMeshes.push_back(subMesh);
		beginIndex += mat.numFaceVertices;
	}
	initMaterials = materials;
	mulMaterialFactors.resize(materials.size());
	addMaterialFactors.resize(materials.size());
	nodes.reserve(pmx.bones.size());
	for (const auto& bone : pmx.bones) {
		auto node = std::make_shared<Node>();
		node->index = static_cast<uint32_t>(nodes.size());
		node->name = bone.name;
		nodes.emplace_back(std::move(node));
	}
	for (size_t i = 0; i < pmx.bones.size(); i++) {
		const auto& bone = pmx.bones[i];
		auto* node = nodes[i].get();
		glm::vec3 localPos = bone.position;
		if (bone.parentBoneIndex != -1) {
			auto parentNode = nodes[bone.parentBoneIndex];
			parentNode->AddChild(nodes[i]);
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
			auto appendNodePtr = nodes[bone.appendBoneIndex];
			float appendWeightValue = bone.appendWeight;
			node->isAppendLocal = appendLocalEnabled;
			node->appendNode = appendNodePtr;
			node->appendWeight = appendWeightValue;
		}
		node->initTranslate = node->translate;
		node->initRotate = node->rotate;
		node->initScale = node->scale;
	}
	transforms.resize(nodes.size());
	sortedNodes.clear();
	sortedNodes.reserve(nodes.size());
	for (auto& node : nodes)
		sortedNodes.emplace_back(*node);
	std::ranges::stable_sort(sortedNodes,
		[](const std::reference_wrapper<Node>& x, const std::reference_wrapper<Node>& y) {
			return x.get().deformDepth < y.get().deformDepth;
		}
	);
	for (size_t i = 0; i < pmx.bones.size(); i++) {
		const auto& bone = pmx.bones[i];
		if (static_cast<uint16_t>(bone.boneFlag) & static_cast<uint16_t>(BoneFlags::Ik)) {
			auto solver = std::make_shared<IkSolver>();
			solver->ikNode = nodes[i];
			nodes[i]->ikSolver = solver;
			solver->ikTarget = nodes[bone.ikTargetBoneIndex];
			for (const auto& [ikBoneIndex, enableLimit, limitMin, limitMax] : bone.ikLinks) {
				auto linkNode = nodes[ikBoneIndex];
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
			ikSolvers.emplace_back(std::move(solver));
		}
	}
	for (const auto& morph : pmx.morphs) {
		auto m = std::make_unique<Morph>();
		m->name = morph.name;
		m->weight = 0.0f;
		m->morphType = morph.morphType;
		if (morph.morphType == MorphType::Position) {
			m->dataIndex = positionMorphs.size();
			std::vector<PositionMorph> morphData;
			for (const auto& [vertexIndex, position] : morph.positionMorph) {
				PositionMorph morphVtx{};
				morphVtx.vertexIndex = vertexIndex;
				morphVtx.position = position * invZ;
				morphData.push_back(morphVtx);
			}
			positionMorphs.emplace_back(std::move(morphData));
		} else if (morph.morphType == MorphType::Uv) {
			m->dataIndex = uvMorphs.size();
			std::vector<UvMorph> morphData;
			for (const auto& [vertexIndex, uv] : morph.uvMorph) {
				UvMorph morphUv{};
				morphUv.vertexIndex = vertexIndex;
				morphUv.uv = uv;
				morphData.push_back(morphUv);
			}
			uvMorphs.emplace_back(std::move(morphData));
		} else if (morph.morphType == MorphType::Material) {
			m->dataIndex = materialMorphs.size();
			materialMorphs.emplace_back(morph.materialMorph);
		} else if (morph.morphType == MorphType::Bone) {
			m->dataIndex = boneMorphs.size();
			std::vector<BoneMorph> boneMorphData;
			for (const auto& [boneIndex, position, quaternion] : morph.boneMorph) {
				auto rot = Util::InvZ(glm::mat3_cast(quaternion));
				BoneMorph boneMorphElem{};
				boneMorphElem.boneIndex = boneIndex;
				boneMorphElem.position = position * invZ;
				boneMorphElem.quaternion = glm::quat_cast(rot);
				boneMorphData.push_back(boneMorphElem);
			}
			boneMorphs.emplace_back(boneMorphData);
		} else if (morph.morphType == MorphType::Group) {
			m->dataIndex = groupMorphs.size();
			groupMorphs.emplace_back(morph.groupMorph);
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
		for (auto& [morphIndex, weight] : groupMorphs[morph->dataIndex]) {
			(void)weight;
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
	for (int32_t i = 0; i < static_cast<int32_t>(morphs.size()); i++) {
		groupMorphStack.clear();
		fixInfiniteGroupMorph(i);
	}
	physics = std::make_unique<Physics>();
	physics->Create();
	for (const auto& pmxRigidBody : pmx.rigidBodies) {
		auto rb = std::make_unique<RigidBody>();
		std::shared_ptr<Node> node;
		if (pmxRigidBody.boneIndex != -1)
			node = nodes[pmxRigidBody.boneIndex];
		rb->Create(pmxRigidBody, this, node);
		physics->world->addRigidBody(rb->rigidBody.get(), 1 << rb->group, rb->groupMask);
		rigidBodies.emplace_back(std::move(rb));
	}
	for (const auto& joint : pmx.joints) {
		if (joint.rigidbodyAIndex != -1 &&
		    joint.rigidbodyBIndex != -1 &&
		    joint.rigidbodyAIndex != joint.rigidbodyBIndex) {
			auto j = std::make_unique<Joint>();
			j->Create(joint,
				rigidBodies[joint.rigidbodyAIndex].get(),
				rigidBodies[joint.rigidbodyBIndex].get()
			);
			physics->world->addConstraint(j->constraint.get());
			joints.emplace_back(std::move(j));
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
	normals.clear();
	uvs.clear();
	vertexBoneInfos.clear();
	indices.clear();
	sortedNodes.clear();
	nodes.clear();
	updateRanges.clear();
	for (const auto& joint : joints)
		physics->world->removeConstraint(joint->constraint.get());
	joints.clear();
	for (const auto& rb : rigidBodies)
		physics->world->removeRigidBody(rb->rigidBody.get());
	rigidBodies.clear();
	physics.reset();
}

void Model::Mul(MaterialMorph& out, const MaterialMorph& val, const float weight) {
	out.diffuse = glm::mix(out.diffuse, out.diffuse * val.diffuse, weight);
	out.specular = glm::mix(out.specular, out.specular * val.specular, weight);
	out.specularPower = glm::mix(out.specularPower, out.specularPower * val.specularPower, weight);
	out.ambient = glm::mix(out.ambient, out.ambient * val.ambient, weight);
	out.edgeColor = glm::mix(out.edgeColor, out.edgeColor * val.edgeColor, weight);
	out.edgeSize = glm::mix(out.edgeSize, out.edgeSize * val.edgeSize, weight);
	out.textureFactor = glm::mix(out.textureFactor, out.textureFactor * val.textureFactor, weight);
	out.sphereTextureFactor = glm::mix(out.sphereTextureFactor, out.sphereTextureFactor * val.sphereTextureFactor, weight);
	out.cartoonTextureFactor = glm::mix(out.cartoonTextureFactor, out.cartoonTextureFactor * val.cartoonTextureFactor, weight);
}

void Model::Add(MaterialMorph& out, const MaterialMorph& val, const float weight) {
	out.diffuse += val.diffuse * weight;
	out.specular += val.specular * weight;
	out.specularPower += val.specularPower * weight;
	out.ambient += val.ambient * weight;
	out.edgeColor += val.edgeColor * weight;
	out.edgeSize += val.edgeSize * weight;
	out.textureFactor += val.textureFactor * weight;
	out.sphereTextureFactor += val.sphereTextureFactor * weight;
	out.cartoonTextureFactor += val.cartoonTextureFactor * weight;
}

void Model::SetupParallelUpdate() {
	if (!parallelUpdateCount)
		parallelUpdateCount = (std::max)(1u, std::thread::hardware_concurrency());
	parallelUpdateCount = std::min<size_t>(parallelUpdateCount, 16);
	updateRanges.resize(parallelUpdateCount);
	parallelUpdateFutures.resize(parallelUpdateCount - 1);
	const size_t totalVertexCount = positions.size();
	constexpr size_t lowerVertexCount = 1000;
	if (totalVertexCount < updateRanges.size() * lowerVertexCount) {
		const size_t numRanges = (totalVertexCount + lowerVertexCount - 1) / lowerVertexCount;
		for (size_t i = 0; i < updateRanges.size(); i++) {
			auto& [vertexOffset, vertexCount] = updateRanges[i];
			if (i < numRanges) {
				vertexOffset = i * lowerVertexCount;
				vertexCount  = (std::min)(lowerVertexCount, totalVertexCount - vertexOffset);
			} else {
				vertexOffset = 0;
				vertexCount = 0;
			}
		}
		return;
	}
	const size_t numVertexCount = totalVertexCount / updateRanges.size();
	size_t offset = 0;
	for (size_t i = 0; i < updateRanges.size(); i++) {
		auto& [vertexOffset, vertexCount] = updateRanges[i];
		vertexOffset = offset;
		vertexCount  = numVertexCount + (i == 0 ? totalVertexCount % updateRanges.size() : 0);
		offset += vertexCount;
	}
}

void Model::Update(const UpdateRange& range) {
	const auto* position = positions.data() + range.vertexOffset;
	const auto* normal = normals.data() + range.vertexOffset;
	const auto* uv = uvs.data() + range.vertexOffset;
	const auto* morphPos = morphPositions.data() + range.vertexOffset;
	const auto* morphUv = morphUVs.data() + range.vertexOffset;
	const auto* vtxInfo = vertexBoneInfos.data() + range.vertexOffset;
	auto* updatePos = updatePositions.data() + range.vertexOffset;
	auto* updateNormal = updateNormals.data() + range.vertexOffset;
	auto* updateUv = updateUVs.data() + range.vertexOffset;
	for (size_t i = 0; i < range.vertexCount;
		i++, vtxInfo++, position++, normal++, uv++, morphPos++, morphUv++, updatePos++, updateNormal++, updateUv++) {
		glm::mat4 m;
		switch (vtxInfo->weightType) {
			case WeightType::BoneDeform1: {
				m = transforms[vtxInfo->boneIndices[0]];
				break;
			}
			case WeightType::BoneDeform2: {
				const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
				const auto w0 = vtxInfo->boneWeights[0], w1 = vtxInfo->boneWeights[1];
				m = transforms[i0] * w0 + transforms[i1] * w1;
				break;
			}
			case WeightType::BoneDeform4: {
				const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
				const auto i2 = vtxInfo->boneIndices[2], i3 = vtxInfo->boneIndices[3];
				const auto w0 = vtxInfo->boneWeights[0], w1 = vtxInfo->boneWeights[1];
				const auto w2 = vtxInfo->boneWeights[2], w3 = vtxInfo->boneWeights[3];
				m = transforms[i0] * w0 + transforms[i1] * w1 + transforms[i2] * w2 + transforms[i3] * w3;
				break;
			}
			case WeightType::SphericalDeform: {
				const auto i0 = vtxInfo->boneIndices[0], i1 = vtxInfo->boneIndices[1];
				const auto w0 = vtxInfo->boneWeights[0], w1 = 1.0f - w0;
				const auto center = vtxInfo->sphericalDeformC, cr0 = vtxInfo->sphericalDeformR0, cr1 = vtxInfo->sphericalDeformR1;
				const auto q0 = glm::quat_cast(nodes[i0]->global);
				const auto q1 = glm::quat_cast(nodes[i1]->global);
				const auto rotMat = glm::mat3_cast(glm::slerp(q0, q1, w1));
				const auto m0 = transforms[i0], m1 = transforms[i1];
				const auto pos = *position + *morphPos;
				*updatePos = rotMat * (pos - center)
				+ glm::vec3(m0 * glm::vec4(cr0, 1)) * w0
				+ glm::vec3(m1 * glm::vec4(cr1, 1)) * w1;
				*updateNormal = rotMat * *normal;
				break;
			}
			case WeightType::QuaternionDeform: {
				glm::dualquat dq[4]{};
				float w[4] = {};
				for (int bi = 0; bi < 4; bi++) {
					auto boneId = vtxInfo->boneIndices[bi];
					if (boneId != -1) {
						dq[bi] = glm::normalize(glm::dualquat_cast(glm::mat3x4(glm::transpose(transforms[boneId]))));
						w[bi] = vtxInfo->boneWeights[bi];
					}
				}
				if (glm::dot(dq[0].real, dq[1].real) < 0)
					w[1] *= -1.0f;
				if (glm::dot(dq[0].real, dq[2].real) < 0)
					w[2] *= -1.0f;
				if (glm::dot(dq[0].real, dq[3].real) < 0)
					w[3] *= -1.0f;
				auto blendDq = glm::normalize(w[0] * dq[0] + w[1] * dq[1] + w[2] * dq[2] + w[3] * dq[3]);
				m = glm::transpose(glm::mat3x4_cast(blendDq));
				break;
			}
			default:
				break;
		}
		if (WeightType::SphericalDeform != vtxInfo->weightType) {
			*updatePos = glm::vec3(m * glm::vec4(*position + *morphPos, 1));
			*updateNormal = glm::normalize(glm::mat3(m) * *normal);
		}
		*updateUv = *uv + glm::vec2(morphUv->x, morphUv->y);
	}
}

void Model::EvalMorph(const Morph* morph, const float morphWeight) {
	if (std::abs(morphWeight) <= std::numeric_limits<float>::epsilon())
		return;
	switch (morph->morphType) {
		case MorphType::Position:
			MorphPosition(positionMorphs[morph->dataIndex], morphWeight);
			break;
		case MorphType::Uv:
			MorphUv(uvMorphs[morph->dataIndex], morphWeight);
			break;
		case MorphType::Material:
			MorphMaterial(materialMorphs[morph->dataIndex], morphWeight);
			break;
		case MorphType::Bone:
			MorphBone(boneMorphs[morph->dataIndex], morphWeight);
			break;
		case MorphType::Group: {
			for (const auto& [morphIndex, weight] : groupMorphs[morph->dataIndex]) {
				if (morphIndex == -1)
					continue;
				EvalMorph(morphs[morphIndex].get(), weight * morphWeight);
			}
			break;
		}
		case MorphType::AddUv1:
			break;
		case MorphType::AddUv2:
			break;
		case MorphType::AddUv3:
			break;
		case MorphType::AddUv4:
			break;
		case MorphType::Flip:
			break;
		case MorphType::Impulse:
			break;
	}
}

void Model::MorphPosition(const std::vector<PositionMorph>& morphData, const float weight) {
	for (const auto& [vertexIndex, position] : morphData)
		morphPositions[vertexIndex] += position * weight;
}

void Model::MorphUv(const std::vector<UvMorph>& morphData, const float weight) {
	for (const auto& [vertexIndex, uv] : morphData)
		morphUVs[vertexIndex] += uv * weight;
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
		auto& mul = mulMaterialFactors[i];
		mul = initMul;
		mul.diffuse       = initMaterials[i].diffuse;
		mul.specular      = initMaterials[i].specular;
		mul.specularPower = initMaterials[i].specularPower;
		mul.ambient       = initMaterials[i].ambient;
		addMaterialFactors[i] = initAdd;
	}
}

void Model::EndMorphMaterial() {
	for (size_t i = 0; i < materials.size(); i++) {
		auto& mat = materials[i];
		const auto& mul = mulMaterialFactors[i];
		const auto& add = addMaterialFactors[i];
		auto matFactor = mul;
		Add(matFactor, add, 1.0f);
		mat.diffuse        = matFactor.diffuse;
		mat.specular       = matFactor.specular;
		mat.specularPower  = matFactor.specularPower;
		mat.ambient        = matFactor.ambient;
		mat.textureMulFactor   = mul.textureFactor;
		mat.textureAddFactor   = add.textureFactor;
		mat.sphereTextureMulFactor = mul.sphereTextureFactor;
		mat.sphereTextureAddFactor = add.sphereTextureFactor;
		mat.cartoonTextureMulFactor = mul.cartoonTextureFactor;
		mat.cartoonTextureAddFactor = add.cartoonTextureFactor;
	}
}

void Model::MorphMaterial(const std::vector<MaterialMorph>& morphData, const float weight) {
	for (const auto& matMorph : morphData) {
		auto Apply = [&](const size_t mi) {
			switch (matMorph.opType) {
				case OpType::Mul: Mul(mulMaterialFactors[mi], matMorph, weight); break;
				case OpType::Add: Add(addMaterialFactors[mi], matMorph, weight); break;
			}
		};
		if (matMorph.materialIndex != -1) {
			Apply(static_cast<size_t>(matMorph.materialIndex));
		} else {
			for (size_t i = 0; i < materials.size(); i++)
				Apply(i);
		}
	}
}

void Model::MorphBone(const std::vector<BoneMorph>& morphData, const float weight) const {
	for (const auto& [boneIndex, position, quaternion] : morphData) {
		auto* node = nodes[boneIndex].get();
		node->translate += position * weight;
		const glm::quat q = glm::slerp(glm::quat(1,0,0,0), quaternion, weight);
		node->rotate = glm::normalize(q * node->rotate);
	}
}

