#include "Core/Model/ModelLoader.h"

#include "Core/Model/ModelPose.h"
#include "Core/Parser/BinaryReader.h"
#include "Core/Parser/PmxParser.h"
#include "Util.h"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace Chrivent {
	std::expected<void, ModelLoadError> ModelLoader::ValidateSupportedFeatures(const PmxParser::PmxData& pmxData) {
		if (!pmxData.softBodies.empty()) {
			return std::unexpected(ModelLoadError{
				ModelLoadErrorCode::UnsupportedFeature,
				"현재 런타임은 PMX 소프트바디를 지원하지 않습니다."
			});
		}
		for (const auto& morph : pmxData.morphs) {
			switch (morph.morphType) {
				case MorphType::Group:
				case MorphType::Position:
				case MorphType::Bone:
				case MorphType::Uv:
				case MorphType::Material:
					break;
				default:
					return std::unexpected(ModelLoadError{
						ModelLoadErrorCode::UnsupportedFeature,
						"현재 런타임이 지원하지 않는 모프가 포함되어 있습니다: " + morph.name
					});
			}
		}
		for (const auto& joint : pmxData.joints) {
			if (joint.type != JointType::SpringDof6) {
				return std::unexpected(ModelLoadError{
					ModelLoadErrorCode::UnsupportedFeature,
					"현재 런타임은 6DoF 스프링 조인트만 지원합니다: " + joint.name
				});
			}
		}
		return {};
	}

	RigidBodyDefinition ModelLoader::CreateRigidBodyDefinition(const PmxParser::PmxRigidbody& rigidBody) {
		RigidBodyDefinition definition{
			.shapeSize = rigidBody.shapeSize,
			.translate = rigidBody.translate,
			.rotate = rigidBody.rotate,
			.mass = rigidBody.mass,
			.translateDamping = rigidBody.translateDimmer,
			.rotateDamping = rigidBody.rotateDimmer,
			.restitution = rigidBody.repulsion,
			.friction = rigidBody.friction,
			.group = rigidBody.group,
			.groupMask = rigidBody.collisionGroup
		};
		switch (rigidBody.shape) {
			case Shape::Sphere: definition.shape = RigidBodyShape::Sphere; break;
			case Shape::Box: definition.shape = RigidBodyShape::Box; break;
			case Shape::Capsule: definition.shape = RigidBodyShape::Capsule; break;
		}
		switch (rigidBody.op) {
			case Operation::Static: definition.operation = RigidBodyOperation::Static; break;
			case Operation::Dynamic: definition.operation = RigidBodyOperation::Dynamic; break;
			case Operation::DynamicAndBoneMerge:
				definition.operation = RigidBodyOperation::DynamicAndBoneMerge;
				break;
		}
		return definition;
	}

	JointDefinition ModelLoader::CreateJointDefinition(const PmxParser::PmxJoint& joint) {
		return {
			.translate = joint.translate,
			.rotate = joint.rotate,
			.translateLowerLimit = joint.translateLowerLimit,
			.translateUpperLimit = joint.translateUpperLimit,
			.rotateLowerLimit = joint.rotateLowerLimit,
			.rotateUpperLimit = joint.rotateUpperLimit,
			.springTranslateFactor = joint.springTranslateFactor,
			.springRotateFactor = joint.springRotateFactor
		};
	}

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
			const uint8_t influenceCount = v.weightType == WeightType::BoneDeform1 ? 1
				: v.weightType == WeightType::BoneDeform2 || v.weightType == WeightType::SphericalDeform ? 2 : 4;
			int32_t fallbackBoneIndex = 0;
			for (uint8_t index = 0; index < influenceCount; index++) {
				if (v.boneIndices[index] >= 0) {
					fallbackBoneIndex = v.boneIndices[index];
					break;
				}
			}
			for (uint8_t index = 0; index < 4; index++) {
				vtxBoneInfo.boneIndices[index] = v.boneIndices[index] >= 0
					? v.boneIndices[index] : fallbackBoneIndex;
				vtxBoneInfo.boneWeights[index] = v.boneWeights[index];
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
		model.geometryData.updatePositions = model.geometryData.positions;
		model.geometryData.updateNormals = model.geometryData.normals;
		model.geometryData.updateUVs = model.geometryData.uvs;
		model.geometryData.previousPositions = model.geometryData.positions;
	}

	void ModelLoader::LoadFaces(const PmxParser::PmxData& pmxData) const {
		model.geometryData.indexElementSize = pmxData.header.vertexIndexSize;
		model.geometryData.indices.resize(pmxData.faces.size() * 3 * model.geometryData.indexElementSize);
		model.geometryData.indexCount = pmxData.faces.size() * 3;
		auto FillIndices = [&]<typename Index>() {
			std::vector<Index> indices(model.geometryData.indexCount);
			size_t index = 0;
			for (const auto& [tri] : pmxData.faces) {
				indices[index++] = static_cast<Index>(tri[2]);
				indices[index++] = static_cast<Index>(tri[1]);
				indices[index++] = static_cast<Index>(tri[0]);
			}
			if (!indices.empty()) {
				std::memcpy(model.geometryData.indices.data(), indices.data(),
					indices.size() * sizeof(Index));
			}
		};
		switch (model.geometryData.indexElementSize) {
			case 1: FillIndices.operator()<uint8_t>(); break;
			case 2: FillIndices.operator()<uint16_t>(); break;
			case 4: FillIndices.operator()<uint32_t>(); break;
			default: break;
		}
	}

	void ModelLoader::LoadMaterials(
		const PmxParser::PmxData& pmxData,
		const std::filesystem::path& modelDir,
		const std::filesystem::path& defaultToonTextureDir) const {
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
					m.toonTexture = defaultToonTextureDir / ss.str();
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
			node->index = static_cast<uint32_t>(model.skeletonData.nodes.size());
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
		std::vector<uint8_t> visitStates(model.morphData.morphs.size());
		std::vector<std::pair<int32_t, size_t>> traversalStack;
		for (size_t rootIndex = 0; rootIndex < model.morphData.morphs.size(); rootIndex++) {
			const auto* rootMorph = model.morphData.morphs[rootIndex].get();
			if (rootMorph->morphType != MorphType::Group || visitStates[rootIndex] != 0)
				continue;
			visitStates[rootIndex] = 1;
			traversalStack.emplace_back(static_cast<int32_t>(rootIndex), 0);
			while (!traversalStack.empty()) {
				auto& [morphIndex, nextChildIndex] = traversalStack.back();
				const auto* morph = model.morphData.morphs[morphIndex].get();
				auto& children = model.morphData.groupMorphs[morph->dataIndex];
				if (nextChildIndex >= children.size()) {
					visitStates[morphIndex] = 2;
					traversalStack.pop_back();
					continue;
				}
				auto& childIndex = children[nextChildIndex++].morphIndex;
				if (childIndex < 0)
					continue;
				const auto* childMorph = model.morphData.morphs[childIndex].get();
				if (childMorph->morphType != MorphType::Group)
					continue;
				if (visitStates[childIndex] == 1) {
					childIndex = -1;
					continue;
				}
				if (visitStates[childIndex] == 0) {
					visitStates[childIndex] = 1;
					traversalStack.emplace_back(childIndex, 0);
				}
			}
		}
	}

	void ModelLoader::LoadPhysics(const PmxParser::PmxData& pmxData) const {
		if (pmxData.rigidBodies.empty())
			return;
		model.physicsData.physics = std::make_unique<Physics>();
		for (const auto& pmxRigidBody : pmxData.rigidBodies) {
			std::shared_ptr<Node> node;
			if (pmxRigidBody.boneIndex != -1)
				node = model.skeletonData.nodes[pmxRigidBody.boneIndex];
			auto rb = std::make_unique<RigidBody>(CreateRigidBodyDefinition(pmxRigidBody), node);
			model.physicsData.physics->AddRigidBody(
				*rb->GetRigidBody(), rb->GetGroup(), rb->GetGroupMask());
			model.physicsData.rigidBodies.emplace_back(std::move(rb));
		}
		for (const auto& joint : pmxData.joints) {
			if (joint.rigidbodyAIndex != -1 &&
				joint.rigidbodyBIndex != -1 &&
				joint.rigidbodyAIndex != joint.rigidbodyBIndex) {
				auto j = std::make_unique<Joint>(CreateJointDefinition(joint),
					*model.physicsData.rigidBodies[joint.rigidbodyAIndex],
					*model.physicsData.rigidBodies[joint.rigidbodyBIndex]);
				model.physicsData.physics->AddConstraint(*j->GetConstraint());
				model.physicsData.joints.emplace_back(std::move(j));
			}
		}
	}

	std::expected<void, ModelLoadError> ModelLoader::Load(const std::filesystem::path& filepath,
		const std::filesystem::path& defaultToonTextureDir) const {
		PmxParser pmx;
		const auto parseResult = pmx.ReadFile(filepath);
		if (!parseResult) {
			return std::unexpected(ModelLoadError{
				ModelLoadErrorCode::Parse,
				"PMX 파일을 읽지 못했습니다: " + BinaryReader::FormatParseError(parseResult.error())
			});
		}
		const auto& pmxData = pmx.GetData();
		const auto supportResult = ValidateSupportedFeatures(pmxData);
		if (!supportResult)
			return std::unexpected(supportResult.error());
		Model loadedModel;
		const ModelLoader loadedModelLoader(loadedModel);
		loadedModel.infoData.modelName = pmxData.info.modelName;
		loadedModel.infoData.englishModelName = pmxData.info.englishModelName;
		loadedModel.infoData.comment = pmxData.info.comment;
		loadedModel.infoData.englishComment = pmxData.info.englishComment;
		const std::filesystem::path modelDir = filepath.parent_path();
		constexpr glm::vec3 invZ(1, 1, -1);
		loadedModelLoader.LoadVertices(pmxData, invZ);
		loadedModelLoader.LoadFaces(pmxData);
		loadedModelLoader.LoadMaterials(pmxData, modelDir, defaultToonTextureDir);
		loadedModelLoader.LoadNodes(pmxData, invZ);
		loadedModelLoader.LoadMorphs(pmxData, invZ);
		loadedModelLoader.FixInfiniteGroupMorphs();
		loadedModelLoader.LoadPhysics(pmxData);
		const ModelPose pose(loadedModel);
		pose.ResetPhysics();
		model.Swap(loadedModel);
		return {};
	}
}
