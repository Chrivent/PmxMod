#include "Core/Model/ModelLoader.h"

#include "Core/Model/Model.h"
#include "Core/Model/ModelCoordinateConverter.h"
#include "Core/Parser/BinaryReader.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

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
				case PmxParser::MorphType::Group:
				case PmxParser::MorphType::Position:
				case PmxParser::MorphType::Bone:
				case PmxParser::MorphType::Uv:
				case PmxParser::MorphType::Material:
					break;
				default:
					return std::unexpected(ModelLoadError{
						ModelLoadErrorCode::UnsupportedFeature,
						"현재 런타임이 지원하지 않는 모프가 포함되어 있습니다: " + morph.name
					});
			}
		}
		for (const auto& joint : pmxData.joints) {
			if (joint.type != PmxParser::JointType::SpringDof6) {
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
			.nodeIndex = rigidBody.boneIndex,
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
			case PmxParser::Shape::Sphere: definition.shape = RigidBodyShape::Sphere; break;
			case PmxParser::Shape::Box: definition.shape = RigidBodyShape::Box; break;
			case PmxParser::Shape::Capsule: definition.shape = RigidBodyShape::Capsule; break;
		}
		switch (rigidBody.op) {
			case PmxParser::Operation::Static: definition.operation = RigidBodyOperation::Static; break;
			case PmxParser::Operation::Dynamic: definition.operation = RigidBodyOperation::Dynamic; break;
			case PmxParser::Operation::DynamicAndBoneMerge:
				definition.operation = RigidBodyOperation::DynamicAndBoneMerge;
				break;
		}
		return definition;
	}

	JointDefinition ModelLoader::CreateJointDefinition(const PmxParser::PmxJoint& joint) {
		return {
			.rigidBodyAIndex = joint.rigidbodyAIndex,
			.rigidBodyBIndex = joint.rigidbodyBIndex,
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

	WeightType ModelLoader::ConvertWeightType(const PmxParser::WeightType weightType) {
		switch (weightType) {
			case PmxParser::WeightType::BoneDeform1: return WeightType::BoneDeform1;
			case PmxParser::WeightType::BoneDeform2: return WeightType::BoneDeform2;
			case PmxParser::WeightType::BoneDeform4: return WeightType::BoneDeform4;
			case PmxParser::WeightType::SphericalDeform: return WeightType::SphericalDeform;
			case PmxParser::WeightType::QuaternionDeform: return WeightType::QuaternionDeform;
		}
		std::unreachable();
	}

	SphereMode ModelLoader::ConvertSphereMode(const PmxParser::SphereMode sphereMode) {
		switch (sphereMode) {
			case PmxParser::SphereMode::None: return SphereMode::None;
			case PmxParser::SphereMode::Mul: return SphereMode::Mul;
			case PmxParser::SphereMode::Add: return SphereMode::Add;
			case PmxParser::SphereMode::SubTexture: return SphereMode::SubTexture;
		}
		std::unreachable();
	}

	MorphType ModelLoader::ConvertMorphType(const PmxParser::MorphType morphType) {
		switch (morphType) {
			case PmxParser::MorphType::Group: return MorphType::Group;
			case PmxParser::MorphType::Position: return MorphType::Position;
			case PmxParser::MorphType::Bone: return MorphType::Bone;
			case PmxParser::MorphType::Uv: return MorphType::Uv;
			case PmxParser::MorphType::AddUv1: return MorphType::AddUv1;
			case PmxParser::MorphType::AddUv2: return MorphType::AddUv2;
			case PmxParser::MorphType::AddUv3: return MorphType::AddUv3;
			case PmxParser::MorphType::AddUv4: return MorphType::AddUv4;
			case PmxParser::MorphType::Material: return MorphType::Material;
			case PmxParser::MorphType::Flip: return MorphType::Flip;
			case PmxParser::MorphType::Impulse: return MorphType::Impulse;
		}
		std::unreachable();
	}

	OpType ModelLoader::ConvertOpType(const PmxParser::OpType opType) {
		switch (opType) {
			case PmxParser::OpType::Mul: return OpType::Mul;
			case PmxParser::OpType::Add: return OpType::Add;
		}
		std::unreachable();
	}

	void ModelLoader::LoadVertices(Model& model, const PmxParser::PmxData& pmxData, const glm::vec3& invZ) {
		size_t vertexCount = pmxData.vertices.size();
		model.geometryData.positions.reserve(vertexCount);
		model.geometryData.normals.reserve(vertexCount);
		model.geometryData.uvs.reserve(vertexCount);
		model.geometryData.vertexBoneInfos.reserve(vertexCount);
		if (vertexCount == 0) {
			model.geometryData.bboxMin = glm::vec3(0);
			model.geometryData.bboxMax = glm::vec3(0);
		} else {
			model.geometryData.bboxMax = glm::vec3(-std::numeric_limits<float>::max());
			model.geometryData.bboxMin = glm::vec3(std::numeric_limits<float>::max());
		}
		for (const auto& v : pmxData.vertices) {
			glm::vec3 pos = v.position * invZ;
			model.geometryData.positions.push_back(pos);
			const glm::vec3 normal = v.normal * invZ;
			const float normalLengthSquared = glm::dot(normal, normal);
			model.geometryData.normals.push_back(normalLengthSquared > std::numeric_limits<float>::epsilon()
				? normal / std::sqrt(normalLengthSquared) : glm::vec3(0));
			model.geometryData.uvs.emplace_back(v.uv.x, 1.0f - v.uv.y);
			Vertex vtxBoneInfo{};
			const uint8_t influenceCount = v.weightType == PmxParser::WeightType::BoneDeform1 ? 1
				: v.weightType == PmxParser::WeightType::BoneDeform2 ||
				v.weightType == PmxParser::WeightType::SphericalDeform ? 2 : 4;
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
			vtxBoneInfo.weightType = ConvertWeightType(v.weightType);
			switch (v.weightType) {
				case PmxParser::WeightType::BoneDeform2:
					vtxBoneInfo.boneWeights[1] = 1.0f - vtxBoneInfo.boneWeights[0];
					break;
				case PmxParser::WeightType::SphericalDeform: {
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

	void ModelLoader::LoadFaces(Model& model, const PmxParser::PmxData& pmxData) {
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

	void ModelLoader::LoadMaterials(Model& model, const PmxParser::PmxData& pmxData,
		const std::filesystem::path& modelDir,
		const std::filesystem::path& defaultToonTextureDir) {
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
			m.bothFace = PmxParser::ContainsFlag(mat.drawMode, PmxParser::DrawModeFlags::BothFace);
			m.edgeFlag = PmxParser::ContainsFlag(mat.drawMode, PmxParser::DrawModeFlags::DrawEdge) ? 1 : 0;
			m.groundShadow = PmxParser::ContainsFlag(mat.drawMode, PmxParser::DrawModeFlags::GroundShadow);
			m.shadowCaster = PmxParser::ContainsFlag(mat.drawMode, PmxParser::DrawModeFlags::CastSelfShadow);
			m.shadowReceiver = PmxParser::ContainsFlag(mat.drawMode, PmxParser::DrawModeFlags::ReceiveSelfShadow);
			m.edgeSize = mat.edgeSize;
			m.edgeColor = mat.edgeColor;
			if (mat.textureIndex != -1)
				m.texture = texturePaths[mat.textureIndex];
			if (mat.toonMode == PmxParser::ToonMode::Common) {
				if (mat.toonTextureIndex != -1) {
					std::stringstream ss;
					ss << "toon" << std::setfill('0') << std::setw(2) << (mat.toonTextureIndex + 1) << ".bmp";
					m.toonTexture = defaultToonTextureDir / ss.str();
				}
			} else if (mat.toonMode == PmxParser::ToonMode::Separate) {
				if (mat.toonTextureIndex != -1)
					m.toonTexture = texturePaths[mat.toonTextureIndex];
			}
			if (mat.sphereTextureIndex != -1) {
				m.spTexture = texturePaths[mat.sphereTextureIndex];
				m.spTextureMode = ConvertSphereMode(mat.sphereMode);
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

	void ModelLoader::LoadNodes(Model& model, const PmxParser::PmxData& pmxData, const glm::vec3& invZ) {
		model.skeletonData.ReserveNodes(pmxData.bones.size());
		for (const auto& bone : pmxData.bones) {
			auto node = std::make_unique<Node>();
			node->index = static_cast<uint32_t>(model.skeletonData.GetNodes().size());
			node->name = bone.name;
			model.skeletonData.AddNode(std::move(node));
		}
		const auto& nodes = model.skeletonData.GetNodes();
		for (size_t i = 0; i < pmxData.bones.size(); i++) {
			const auto& bone = pmxData.bones[i];
			auto* node = nodes[i].get();
			glm::vec3 localPos = bone.position;
			if (bone.parentBoneIndex != -1) {
				Node* parentNode = nodes[bone.parentBoneIndex].get();
				parentNode->AddChild(*nodes[i]);
				localPos -= pmxData.bones[bone.parentBoneIndex].position;
			}
			localPos.z *= -1;
			node->translate = localPos;
			node->global = glm::translate(glm::mat4(1), bone.position * invZ);
			node->inverseInit = glm::inverse(node->global);
			node->deformDepth = bone.deformDepth;
			bool deformAfterPhysics = PmxParser::ContainsFlag(bone.boneFlag, PmxParser::BoneFlags::DeformAfterPhysics);
			node->isDeformAfterPhysics = deformAfterPhysics;
			bool appendRotateEnabled = PmxParser::ContainsFlag(bone.boneFlag, PmxParser::BoneFlags::AppendRotate);
			bool appendTranslateEnabled = PmxParser::ContainsFlag(bone.boneFlag, PmxParser::BoneFlags::AppendTranslate);
			node->isAppendRotate = appendRotateEnabled;
			node->isAppendTranslate = appendTranslateEnabled;
			if ((appendRotateEnabled || appendTranslateEnabled) && bone.appendBoneIndex != -1) {
				bool appendLocalEnabled = PmxParser::ContainsFlag(bone.boneFlag, PmxParser::BoneFlags::AppendLocal);
				float appendWeightValue = bone.appendWeight;
				node->isAppendLocal = appendLocalEnabled;
				node->appendNode = nodes[bone.appendBoneIndex].get();
				node->appendWeight = appendWeightValue;
			}
			node->initTranslate = node->translate;
			node->initRotate = node->rotate;
			node->initScale = node->scale;
		}
		model.skeletonData.transforms.resize(nodes.size());
		model.skeletonData.sortedNodes.clear();
		model.skeletonData.sortedNodes.reserve(nodes.size());
		for (const auto& node : nodes)
			model.skeletonData.sortedNodes.emplace_back(*node);
		std::ranges::stable_sort(model.skeletonData.sortedNodes,
		[](const std::reference_wrapper<Node>& x, const std::reference_wrapper<Node>& y) {
			return x.get().deformDepth < y.get().deformDepth;
		});
		for (size_t i = 0; i < pmxData.bones.size(); i++) {
			const auto& bone = pmxData.bones[i];
			if (PmxParser::ContainsFlag(bone.boneFlag, PmxParser::BoneFlags::Ik)) {
				auto solver = std::make_unique<IkSolver>();
				solver->ikNode = nodes[i].get();
				nodes[i]->ikSolver = solver.get();
				solver->ikTarget = nodes[bone.ikTargetBoneIndex].get();
				for (const auto& [ikBoneIndex, enableLimit,
					limitMin, limitMax] : bone.ikLinks) {
					Node* linkNode = nodes[ikBoneIndex].get();
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
				model.skeletonData.AddIkSolver(std::move(solver));
			}
		}
		model.skeletonData.displayFrames.reserve(pmxData.displayFrames.size());
		for (const auto& displayFrame : pmxData.displayFrames) {
			ModelDisplayFrame frame;
			frame.name = displayFrame.name.empty() ? displayFrame.englishName : displayFrame.name;
			for (const auto& [type, index] : displayFrame.targets) {
				if (index < 0)
					continue;
				if (type == PmxParser::TargetType::BoneIndex && index < pmxData.bones.size())
					frame.boneIndices.emplace_back(index);
				else if (type == PmxParser::TargetType::MorphIndex && index < pmxData.morphs.size())
					frame.morphIndices.emplace_back(index);
			}
			model.skeletonData.displayFrames.emplace_back(std::move(frame));
		}
	}

	void ModelLoader::LoadMorphs(Model& model, const PmxParser::PmxData& pmxData, const glm::vec3& invZ) {
		for (const auto& morph : pmxData.morphs) {
			auto m = std::make_unique<Morph>();
			m->name = morph.name;
			m->morphType = ConvertMorphType(morph.morphType);
			switch (morph.morphType) {
				case PmxParser::MorphType::Position: {
					m->dataIndex = model.morphData.positionMorphs.size();
					std::vector<PositionMorph> morphData;
					morphData.reserve(morph.positionMorph.size());
					for (const auto& [vertexIndex, position] : morph.positionMorph)
						morphData.push_back({ vertexIndex, position * invZ });
					model.morphData.positionMorphs.emplace_back(std::move(morphData));
					break;
				}
				case PmxParser::MorphType::Uv: {
					m->dataIndex = model.morphData.uvMorphs.size();
					std::vector<UvMorph> morphData;
					morphData.reserve(morph.uvMorph.size());
					for (const auto& [vertexIndex, uv] : morph.uvMorph)
						morphData.push_back({vertexIndex, uv});
					model.morphData.uvMorphs.emplace_back(std::move(morphData));
					break;
				}
				case PmxParser::MorphType::Material: {
					m->dataIndex = model.morphData.materialMorphs.size();
					std::vector<MaterialMorph> morphData;
					morphData.reserve(morph.materialMorph.size());
					for (const auto& [materialIndex, opType, diffuse
						, specular, specularPower, ambient
						, edgeColor, edgeSize, textureFactor
						, sphereTextureFactor, toonTextureFactor] : morph.materialMorph) {
						morphData.push_back({
							materialIndex, ConvertOpType(opType), diffuse,
							specular, specularPower, ambient,
							edgeColor, edgeSize, textureFactor,
							sphereTextureFactor, toonTextureFactor
						});
					}
					model.morphData.materialMorphs.emplace_back(std::move(morphData));
					break;
				}
				case PmxParser::MorphType::Bone: {
					m->dataIndex = model.morphData.boneMorphs.size();
					std::vector<BoneMorph> boneMorphData;
					boneMorphData.reserve(morph.boneMorph.size());
					for (const auto& [boneIndex, position, quaternion] : morph.boneMorph) {
						const auto rot = ModelCoordinateConverter::ConvertZAxis(
							glm::mat3_cast(glm::normalize(quaternion)));
						boneMorphData.push_back({
							boneIndex, position * invZ, glm::normalize(glm::quat_cast(rot))
						});
					}
					model.morphData.boneMorphs.emplace_back(std::move(boneMorphData));
					break;
				}
				case PmxParser::MorphType::Group: {
					m->dataIndex = model.morphData.groupMorphs.size();
					std::vector<GroupMorph> morphData;
					morphData.reserve(morph.groupMorph.size());
					for (const auto& [morphIndex, weight] : morph.groupMorph)
						morphData.push_back({morphIndex, weight});
					model.morphData.groupMorphs.emplace_back(std::move(morphData));
					break;
				}
				default:
					break;
			}
			model.morphData.AddMorph(std::move(m));
		}
	}

	void ModelLoader::FixInfiniteGroupMorphs(Model& model) {
		const auto& morphs = model.morphData.GetMorphs();
		std::vector<uint8_t> visitStates(morphs.size());
		std::vector<std::pair<int32_t, size_t>> traversalStack;
		for (size_t rootIndex = 0; rootIndex < morphs.size(); rootIndex++) {
			const auto* rootMorph = morphs[rootIndex].get();
			if (rootMorph->morphType != MorphType::Group || visitStates[rootIndex] != 0)
				continue;
			visitStates[rootIndex] = 1;
			traversalStack.emplace_back(static_cast<int32_t>(rootIndex), 0);
			while (!traversalStack.empty()) {
				auto& [morphIndex, nextChildIndex] = traversalStack.back();
				const auto* morph = morphs[morphIndex].get();
				auto& children = model.morphData.groupMorphs[morph->dataIndex];
				if (nextChildIndex >= children.size()) {
					visitStates[morphIndex] = 2;
					traversalStack.pop_back();
					continue;
				}
				auto& childIndex = children[nextChildIndex++].morphIndex;
				if (childIndex < 0)
					continue;
				const auto* childMorph = morphs[childIndex].get();
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

	std::expected<void, ModelLoadError> ModelLoader::LoadPhysics(Model& model, const PmxParser::PmxData& pmxData) {
		std::vector<RigidBodyDefinition> rigidBodies;
		rigidBodies.reserve(pmxData.rigidBodies.size());
		for (const auto& rigidBody : pmxData.rigidBodies)
			rigidBodies.emplace_back(CreateRigidBodyDefinition(rigidBody));
		std::vector<JointDefinition> joints;
		joints.reserve(pmxData.joints.size());
		for (const auto& joint : pmxData.joints)
			joints.emplace_back(CreateJointDefinition(joint));
		const auto physicsResult = model.InitializePhysics(rigidBodies, joints);
		if (!physicsResult) {
			const auto& [code, definitionIndex, message] = physicsResult.error();
			const std::string item = code == PhysicsErrorCode::InvalidRigidBody ? "강체 " : "조인트 ";
			return std::unexpected(ModelLoadError{
				ModelLoadErrorCode::Physics,
				item + std::to_string(definitionIndex) + ": " + message
			});
		}
		return {};
	}

	std::expected<void, ModelLoadError> ModelLoader::Build(Model& model, const PmxParser::PmxData& pmxData,
		const std::filesystem::path& modelDir, const std::filesystem::path& defaultToonTextureDir) {
		const auto supportResult = ValidateSupportedFeatures(pmxData);
		if (!supportResult)
			return std::unexpected(supportResult.error());
		model.infoData.modelName = pmxData.info.modelName;
		model.infoData.englishModelName = pmxData.info.englishModelName;
		model.infoData.comment = pmxData.info.comment;
		model.infoData.englishComment = pmxData.info.englishComment;
		constexpr glm::vec3 invZ(1, 1, -1);
		LoadVertices(model, pmxData, invZ);
		LoadFaces(model, pmxData);
		LoadMaterials(model, pmxData, modelDir, defaultToonTextureDir);
		LoadNodes(model, pmxData, invZ);
		LoadMorphs(model, pmxData, invZ);
		FixInfiniteGroupMorphs(model);
		const auto physicsResult = LoadPhysics(model, pmxData);
		if (!physicsResult)
			return std::unexpected(physicsResult.error());
		model.ResetPhysics();
		return {};
	}

	std::expected<void, ModelLoadError> ModelLoader::Load(Model& model, const std::filesystem::path& filepath,
		const std::filesystem::path& defaultToonTextureDir) {
		PmxParser parser;
		const auto parseResult = parser.ReadFile(filepath);
		if (!parseResult) {
			return std::unexpected(ModelLoadError{
				ModelLoadErrorCode::Parse,
				"PMX 파일을 읽지 못했습니다: " + BinaryReader::FormatParseError(parseResult.error())
			});
		}
		Model loadedModel;
		const auto buildResult = Build(
			loadedModel, parser.GetData(), filepath.parent_path(), defaultToonTextureDir);
		if (!buildResult)
			return std::unexpected(buildResult.error());
		model.Swap(loadedModel);
		return {};
	}

}
