#include "Core/Animation/Camera/CameraAnimation.h"
#include "Core/Animation/Camera/CameraAnimationBuilder.h"
#include "Core/Animation/Model/AnimationBuilder.h"
#include "Core/Model/Model.h"
#include "Core/Model/ModelAnimator.h"
#include "Core/Model/ModelLoader.h"
#include "Core/Model/ModelSkinning.h"
#include "Core/Model/ModelUpdater.h"

#include <gtest/gtest.h>
#include <fstream>
#include <limits>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>

namespace Chrivent {
	// 카메라 키 생성과 모델 상태 검증을 공유한다.
	class AnimationModelContractTest : public testing::Test {
	protected:
		static CameraAnimationKey CreateCameraKey(const uint32_t frame, const float x) {
			CameraAnimationKey key{};
			key.frame = frame;
			key.interest = glm::vec3(x, 0, 0);
			key.distance = x;
			key.fov = glm::radians(30.0f + x);
			key.ixBezier.Assign(0, 127, 0, 127);
			key.iyBezier.Assign(0, 127, 0, 127);
			key.izBezier.Assign(0, 127, 0, 127);
			key.rotateBezier.Assign(0, 127, 0, 127);
			key.distanceBezier.Assign(0, 127, 0, 127);
			key.fovBezier.Assign(0, 127, 0, 127);
			return key;
		}

		template <typename T>
		static void Append(std::string& bytes, const T& value) {
			bytes.append(reinterpret_cast<const char*>(&value), sizeof(T));
		}

		static std::string BuildMinimalPmx() {
			std::string bytes;
			bytes.append("PMX ", 4);
			Append(bytes, 2.0f);
			constexpr uint8_t headerData[] = {8, 1, 0, 1, 1, 1, 1, 1, 1};
			bytes.append(reinterpret_cast<const char*>(headerData), sizeof(headerData));
			constexpr int32_t emptyCount = 0;
			for (int stringIndex = 0; stringIndex < 4; stringIndex++)
				Append(bytes, emptyCount);
			for (int sectionIndex = 0; sectionIndex < 9; sectionIndex++)
				Append(bytes, emptyCount);
			return bytes;
		}

		static bool IsFinite(const glm::quat& value) {
			return std::isfinite(value.w) && std::isfinite(value.x) &&
				std::isfinite(value.y) && std::isfinite(value.z);
		}
	};

	TEST_F(AnimationModelContractTest, CutsExactlyAtAdjacentCameraFrame) {
		std::vector<CameraAnimationKey> keys;
		keys.emplace_back(CreateCameraKey(0, 0.0f));
		keys.emplace_back(CreateCameraKey(1, 10.0f));
		CameraAnimation animation(std::move(keys));
		const Camera beforeCut = animation.Evaluate(0.5f);
		const Camera atCut = animation.Evaluate(1.0f);
		EXPECT_FLOAT_EQ(beforeCut.interest.x, 0.0f);
		EXPECT_FLOAT_EQ(beforeCut.distance, 0.0f);
		EXPECT_FLOAT_EQ(atCut.interest.x, 10.0f);
		EXPECT_FLOAT_EQ(atCut.distance, 10.0f);
	}

	TEST_F(AnimationModelContractTest, CameraAnimationNormalizesDirectlyProvidedKeys) {
		std::vector<CameraAnimationKey> keys;
		keys.emplace_back(CreateCameraKey(5, 1.0f));
		keys.emplace_back(CreateCameraKey(1, 0.0f));
		keys.emplace_back(CreateCameraKey(5, 2.0f));
		const CameraAnimation animation(std::move(keys));
		ASSERT_EQ(animation.GetKeys().size(), 2);
		EXPECT_EQ(animation.GetKeys()[0].frame, 1);
		EXPECT_EQ(animation.GetKeys()[1].frame, 5);
		EXPECT_FLOAT_EQ(animation.Evaluate(5.0f).interest.x, 2.0f);
	}

	TEST_F(AnimationModelContractTest, ModelAnimationNormalizesDirectlyProvidedKeys) {
		const auto model = std::make_shared<Model>();
		const auto node = std::make_shared<Node>();
		model->skeletonData.AddNode(node);
		std::vector<NodeAnimationKey> keys(3);
		keys[0].frame = 5;
		keys[0].translate = glm::vec3(1, 0, 0);
		keys[1].frame = 1;
		keys[1].translate = glm::vec3(0);
		keys[2].frame = 5;
		keys[2].translate = glm::vec3(2, 0, 0);
		std::vector<NodeAnimationTrack> tracks;
		tracks.push_back({node.get(), std::move(keys)});
		const Animation animation(model, std::move(tracks), {}, {});
		ASSERT_EQ(animation.GetNodeTracks().front().keys.size(), 2);
		animation.Evaluate(5.0f);
		EXPECT_EQ(node->animTranslate, glm::vec3(2, 0, 0));
	}

	TEST_F(AnimationModelContractTest, CalculatesLastFrameAcrossEveryTrackType) {
		const auto model = std::make_shared<Model>();
		const auto node = std::make_shared<Node>();
		const auto ikSolver = std::make_shared<IkSolver>();
		auto morph = std::make_unique<Morph>();
		auto* morphTarget = morph.get();
		model->skeletonData.AddNode(node);
		model->skeletonData.AddIkSolver(ikSolver);
		model->morphData.AddMorph(std::move(morph));
		NodeAnimationKey nodeKey{};
		nodeKey.frame = 3;
		IkAnimationKey ikKey{};
		ikKey.frame = 7;
		MorphAnimationKey morphKey{};
		morphKey.frame = 5;
		const Animation animation(model,
			{{node.get(), {nodeKey}}},
			{{ikSolver.get(), {ikKey}}},
			{{morphTarget, {morphKey}}});
		EXPECT_EQ(animation.CalculateLastFrame(), 7);
	}

	TEST_F(AnimationModelContractTest, EvaluatesLinearBezierEndpointsAndMidpoint) {
		Bezier bezier;
		bezier.Assign(0, 127, 0, 127);
		EXPECT_FLOAT_EQ(bezier.Evaluate(0.0f), 0.0f);
		EXPECT_NEAR(bezier.Evaluate(0.5f), 0.5f, 1.0e-4f);
		EXPECT_FLOAT_EQ(bezier.Evaluate(1.0f), 1.0f);
	}

	TEST_F(AnimationModelContractTest, ResetClearsTemporalGeometryState) {
		Model model;
		model.geometryData.previousPositions.emplace_back(1.0f, 2.0f, 3.0f);
		model.geometryData.bboxMin = glm::vec3(-5.0f);
		model.geometryData.bboxMax = glm::vec3(5.0f);
		model.Reset();
		EXPECT_TRUE(model.geometryData.previousPositions.empty());
		EXPECT_EQ(model.geometryData.bboxMin, glm::vec3(0));
		EXPECT_EQ(model.geometryData.bboxMax, glm::vec3(0));
	}

	TEST_F(AnimationModelContractTest, CreatesOnlyUsefulSkinningRanges) {
		Model model;
		model.geometryData.positions.resize(10);
		ModelSkinning::PrepareUpdate(model, false);
		ASSERT_EQ(ModelUpdater::CalculateSkinningTaskCount(model), 1);
		EXPECT_EQ(model.geometryData.updateRanges.front().vertexOffset, 0);
		EXPECT_EQ(model.geometryData.updateRanges.front().vertexCount, 10);
		model.geometryData.positions.clear();
		ModelSkinning::PrepareUpdate(model, false);
		EXPECT_EQ(ModelUpdater::CalculateSkinningTaskCount(model), 0);
	}

	TEST_F(AnimationModelContractTest, FailedLoadPreservesTheExistingModel) {
		Model model;
		model.infoData.modelName = "기존 모델";
		const auto result = ModelLoader::Load(
			model, "pmxmod_core_test_file_that_does_not_exist.pmx", {});
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ModelLoadErrorCode::Parse);
		EXPECT_EQ(model.infoData.modelName, "기존 모델");
	}

	TEST_F(AnimationModelContractTest, LoadsAnEmptyModelWithAZeroBoundingBox) {
		const auto path = std::filesystem::temp_directory_path() / "pmxmod_empty_model_test.pmx";
		const std::string bytes = BuildMinimalPmx();
		{
			std::ofstream file(path, std::ios::binary);
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}
		Model model;
		const auto result = ModelLoader::Load(model, path, {});
		std::error_code error;
		std::filesystem::remove(path, error);
		ASSERT_TRUE(result);
		EXPECT_EQ(model.geometryData.bboxMin, glm::vec3(0));
		EXPECT_EQ(model.geometryData.bboxMax, glm::vec3(0));
	}

	TEST_F(AnimationModelContractTest, NormalizesVmdBoneRotationsAtTheRuntimeBoundary) {
		auto model = std::make_shared<Model>();
		auto node = std::make_shared<Node>();
		node->name = "A";
		model->skeletonData.AddNode(node);
		VmdParser::VmdData data{};
		auto& motion = data.motions.emplace_back();
		std::memcpy(motion.boneName, "A", 1);
		motion.quaternion = glm::quat(2, 0, 0, 0);
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		ASSERT_EQ(animation->GetNodeTracks().size(), 1);
		ASSERT_EQ(animation->GetNodeTracks().front().keys.size(), 1);
		const glm::quat rotation = animation->GetNodeTracks().front().keys.front().rotate;
		EXPECT_NEAR(glm::length(rotation), 1.0f, 1.0e-6f);
	}

	TEST_F(AnimationModelContractTest, AnimationKeepsItsTargetModelAlive) {
		std::weak_ptr<Model> modelReference;
		std::weak_ptr<Node> nodeReference;
		std::unique_ptr<Animation> animation;
		{
			auto model = std::make_shared<Model>();
			auto node = std::make_shared<Node>();
			node->name = "A";
			model->skeletonData.AddNode(node);
			modelReference = model;
			nodeReference = node;
			VmdParser::VmdData data{};
			auto& motion = data.motions.emplace_back();
			std::memcpy(motion.boneName, "A", 1);
			motion.quaternion = glm::quat(1, 0, 0, 0);
			AnimationBuilder builder(model);
			builder.Build(data);
			animation = builder.TakeAnimation();
		}
		ASSERT_FALSE(modelReference.expired());
		ASSERT_FALSE(nodeReference.expired());
		animation->Evaluate(0, 1);
		animation.reset();
		EXPECT_TRUE(modelReference.expired());
		EXPECT_TRUE(nodeReference.expired());
	}

	TEST_F(AnimationModelContractTest, InvalidatesAnimationBindingsWhenModelStructureChanges) {
		const auto model = std::make_shared<Model>();
		const auto node = std::make_shared<Node>();
		node->name = "A";
		model->skeletonData.AddNode(node);
		VmdParser::VmdData data{};
		auto& motion = data.motions.emplace_back();
		std::memcpy(motion.boneName, "A", 1);
		motion.translate = glm::vec3(3, 0, 0);
		motion.quaternion = glm::quat(1, 0, 0, 0);
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		model->Reset();
		node->animTranslate = glm::vec3(7);
		animation->Evaluate(0);
		EXPECT_EQ(node->animTranslate, glm::vec3(7));
	}

	TEST_F(AnimationModelContractTest, BindsTracksToTheCurrentModelWhenTaken) {
		const auto model = std::make_shared<Model>();
		const auto originalNode = std::make_shared<Node>();
		originalNode->name = "A";
		model->skeletonData.AddNode(originalNode);
		VmdParser::VmdData data{};
		auto& motion = data.motions.emplace_back();
		std::memcpy(motion.boneName, "A", 1);
		motion.translate = glm::vec3(4, 0, 0);
		motion.quaternion = glm::quat(1, 0, 0, 0);
		AnimationBuilder builder(model);
		builder.Build(data);
		model->Reset();
		const auto replacementNode = std::make_shared<Node>();
		replacementNode->name = "A";
		model->skeletonData.AddNode(replacementNode);
		const auto animation = builder.TakeAnimation();
		ASSERT_EQ(animation->GetNodeTracks().size(), 1);
		EXPECT_EQ(animation->GetNodeTracks().front().node, replacementNode.get());
		animation->Evaluate(0);
		EXPECT_EQ(replacementNode->animTranslate, glm::vec3(4, 0, 0));
	}

	TEST_F(AnimationModelContractTest, KeepsTheLastModelKeyAtADuplicateFrame) {
		const auto model = std::make_shared<Model>();
		const auto node = std::make_shared<Node>();
		node->name = "A";
		model->skeletonData.AddNode(node);
		VmdParser::VmdData data{};
		for (const float x : {1.0f, 2.0f}) {
			auto& motion = data.motions.emplace_back();
			std::memcpy(motion.boneName, "A", 1);
			motion.frame = 5;
			motion.translate = glm::vec3(x, 0, 0);
			motion.quaternion = glm::quat(1, 0, 0, 0);
		}
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		ASSERT_EQ(animation->GetNodeTracks().size(), 1);
		ASSERT_EQ(animation->GetNodeTracks().front().keys.size(), 1);
		EXPECT_EQ(animation->GetNodeTracks().front().keys.front().translate.x, 2.0f);
	}

	TEST_F(AnimationModelContractTest, MergesModelKeysAcrossBuildCalls) {
		const auto model = std::make_shared<Model>();
		const auto node = std::make_shared<Node>();
		node->name = "A";
		model->skeletonData.AddNode(node);
		AnimationBuilder builder(model);
		for (const auto& [frame, x] : {std::pair{5u, 1.0f}, std::pair{8u, 2.0f}}) {
			VmdParser::VmdData data{};
			auto& motion = data.motions.emplace_back();
			std::memcpy(motion.boneName, "A", 1);
			motion.frame = frame;
			motion.translate = glm::vec3(x, 0, 0);
			motion.quaternion = glm::quat(1, 0, 0, 0);
			builder.Build(data);
		}
		const auto animation = builder.TakeAnimation();
		ASSERT_EQ(animation->GetNodeTracks().size(), 1);
		const auto& keys = animation->GetNodeTracks().front().keys;
		ASSERT_EQ(keys.size(), 2);
		EXPECT_EQ(keys[0].frame, 5);
		EXPECT_EQ(keys[1].frame, 8);
	}

	TEST_F(AnimationModelContractTest, ResetsModelStateThroughTheUpdaterContract) {
		const auto model = std::make_shared<Model>();
		const auto node = std::make_shared<Node>();
		node->name = "A";
		model->skeletonData.AddNode(node);
		model->skeletonData.sortedNodes.emplace_back(*node);
		VmdParser::VmdData data{};
		auto& motion = data.motions.emplace_back();
		std::memcpy(motion.boneName, "A", 1);
		motion.frame = 12;
		motion.translate = glm::vec3(3, 0, 0);
		motion.quaternion = glm::quat(1, 0, 0, 0);
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		ModelUpdater::ResetPhysicsAtFrame(*model, *animation, 12.0f);
		EXPECT_EQ(node->animTranslate, glm::vec3(3, 0, 0));
		EXPECT_EQ(glm::vec3(node->global[3]), glm::vec3(3, 0, 0));
	}

	TEST_F(AnimationModelContractTest, KeepsTheLastCameraKeyAtADuplicateFrame) {
		VmdParser::VmdData data{};
		for (const float x : {1.0f, 2.0f}) {
			auto& camera = data.cameras.emplace_back();
			camera.frame = 5;
			camera.interest = glm::vec3(x, 0, 0);
			camera.viewAngle = 30;
		}
		const auto animation = CameraAnimationBuilder::Build(data);
		ASSERT_EQ(animation->GetKeys().size(), 1);
		EXPECT_EQ(animation->GetKeys().front().interest.x, 2.0f);
	}

	TEST_F(AnimationModelContractTest, ReusesAnimationBuilderWithoutKeepingTakenTracks) {
		auto model = std::make_shared<Model>();
		auto node = std::make_shared<Node>();
		node->name = "A";
		model->skeletonData.AddNode(node);
		AnimationBuilder builder(model);
		VmdParser::VmdData firstData{};
		auto& firstMotion = firstData.motions.emplace_back();
		std::memcpy(firstMotion.boneName, "A", 1);
		firstMotion.frame = 3;
		firstMotion.quaternion = glm::quat(1, 0, 0, 0);
		builder.Build(firstData);
		const auto firstAnimation = builder.TakeAnimation();
		VmdParser::VmdData secondData{};
		auto& secondMotion = secondData.motions.emplace_back();
		std::memcpy(secondMotion.boneName, "A", 1);
		secondMotion.frame = 7;
		secondMotion.quaternion = glm::quat(1, 0, 0, 0);
		builder.Build(secondData);
		const auto secondAnimation = builder.TakeAnimation();
		ASSERT_EQ(firstAnimation->GetNodeTracks().size(), 1);
		ASSERT_EQ(secondAnimation->GetNodeTracks().size(), 1);
		ASSERT_EQ(firstAnimation->GetNodeTracks().front().keys.size(), 1);
		ASSERT_EQ(secondAnimation->GetNodeTracks().front().keys.size(), 1);
		EXPECT_EQ(firstAnimation->GetNodeTracks().front().keys.front().frame, 3);
		EXPECT_EQ(secondAnimation->GetNodeTracks().front().keys.front().frame, 7);
	}

	TEST_F(AnimationModelContractTest, SkipsPhysicsSynchronizationWithoutAWorld) {
		const auto model = std::make_shared<Model>();
		const auto node = std::make_shared<Node>();
		node->animTranslate = glm::vec3(8);
		node->baseAnimTranslate = glm::vec3(5);
		model->skeletonData.AddNode(node);
		const Animation animation(model, {}, {}, {});
		ModelAnimator::SyncPhysics(*model, animation, 0);
		EXPECT_EQ(node->baseAnimTranslate, glm::vec3(5));
	}

	TEST_F(AnimationModelContractTest, EvaluatesNestedGroupMorphsWithCombinedWeights) {
		Model model;
		model.morphData.morphPositions.resize(1);
		model.morphData.positionMorphs.push_back({{0, glm::vec3(2, 0, 0)}});
		model.morphData.groupMorphs.push_back({{0, 0.5f}});
		auto positionMorph = std::make_unique<Morph>();
		positionMorph->morphType = MorphType::Position;
		positionMorph->dataIndex = 0;
		model.morphData.AddMorph(std::move(positionMorph));
		auto groupMorph = std::make_unique<Morph>();
		groupMorph->morphType = MorphType::Group;
		groupMorph->dataIndex = 0;
		groupMorph->weight = 0.5f;
		model.morphData.AddMorph(std::move(groupMorph));
		model.AccumulateMorphs();
		EXPECT_EQ(model.morphData.morphPositions.front(), glm::vec3(0.5f, 0, 0));
	}

	TEST_F(AnimationModelContractTest, AppliesMaterialMorphEdgeProperties) {
		Model model;
		Material material;
		material.edgeColor = glm::vec4(0.8f, 0.6f, 0.4f, 1.0f);
		material.edgeSize = 2.0f;
		model.materialData.materials.emplace_back(material);
		model.materialData.initMaterials.emplace_back(material);
		model.materialData.mulMaterialFactors.resize(1);
		model.materialData.addMaterialFactors.resize(1);
		MaterialMorph edgeMorph;
		edgeMorph.materialIndex = 0;
		edgeMorph.opType = OpType::Mul;
		edgeMorph.edgeColor = glm::vec4(0.5f);
		edgeMorph.edgeSize = 3.0f;
		model.morphData.materialMorphs.push_back({edgeMorph});
		auto morph = std::make_unique<Morph>();
		morph->morphType = MorphType::Material;
		morph->dataIndex = 0;
		morph->weight = 1.0f;
		model.morphData.AddMorph(std::move(morph));
		model.AccumulateMorphs();
		EXPECT_EQ(model.materialData.materials.front().edgeColor, material.edgeColor * 0.5f);
		EXPECT_FLOAT_EQ(model.materialData.materials.front().edgeSize, material.edgeSize * 3.0f);
	}

	TEST_F(AnimationModelContractTest, OwnsPhysicsThroughTheModelBoundary) {
		Model model;
		RigidBodyDefinition rigidBody{};
		rigidBody.shapeSize = glm::vec3(1);
		rigidBody.groupMask = 0xffff;
		ASSERT_TRUE(model.InitializePhysics({rigidBody}, {}));
		ASSERT_TRUE(model.HasPhysics());
		model.ResetPhysics();
		model.UpdatePhysics(1.0f / 60.0f);
		model.Reset();
		EXPECT_FALSE(model.HasPhysics());
	}

	TEST_F(AnimationModelContractTest, IgnoresInvalidPhysicsElapsedTime) {
		const Model model;
		RigidBodyDefinition rigidBody{};
		rigidBody.shapeSize = glm::vec3(1);
		ASSERT_TRUE(model.InitializePhysics({rigidBody}, {}));
		model.UpdatePhysics(-1.0f);
		model.UpdatePhysics(std::numeric_limits<float>::quiet_NaN());
		EXPECT_TRUE(model.HasPhysics());
	}

	TEST_F(AnimationModelContractTest, RejectsInvalidPhysicsWithoutDiscardingTheCurrentWorld) {
		Model model;
		RigidBodyDefinition validRigidBody{};
		validRigidBody.shapeSize = glm::vec3(1);
		ASSERT_TRUE(model.InitializePhysics({validRigidBody}, {}));
		RigidBodyDefinition invalidRigidBody = validRigidBody;
		invalidRigidBody.group = 16;
		const auto result = model.InitializePhysics({invalidRigidBody}, {});
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, PhysicsErrorCode::InvalidRigidBody);
		EXPECT_EQ(result.error().definitionIndex, 0);
		EXPECT_TRUE(model.HasPhysics());
	}

	TEST_F(AnimationModelContractTest, RejectsInvalidJointReferences) {
		Model model;
		RigidBodyDefinition rigidBody{};
		rigidBody.shapeSize = glm::vec3(1);
		JointDefinition joint{};
		joint.rigidBodyAIndex = 0;
		joint.rigidBodyBIndex = 0;
		const auto result = model.InitializePhysics({rigidBody}, {joint});
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, PhysicsErrorCode::InvalidJoint);
		EXPECT_FALSE(model.HasPhysics());
	}

	TEST_F(AnimationModelContractTest, KeepsZeroNormalsFiniteDuringSkinning) {
		Model model;
		model.geometryData.positions.emplace_back(0.0f);
		model.geometryData.normals.emplace_back(0.0f);
		model.geometryData.uvs.emplace_back(0.0f);
		auto& vertex = model.geometryData.vertexBoneInfos.emplace_back();
		vertex.weightType = WeightType::BoneDeform1;
		vertex.boneIndices[0] = 0;
		vertex.boneWeights[0] = 1.0f;
		model.geometryData.updatePositions.resize(1);
		model.geometryData.updateNormals.resize(1);
		model.geometryData.updateUVs.resize(1);
		model.morphData.morphPositions.resize(1);
		model.morphData.morphUVs.resize(1);
		model.skeletonData.transforms.emplace_back(1.0f);
		ModelSkinning::PrepareUpdate(model, false);
		ModelSkinning::UpdateRange(model, 0);
		const glm::vec3 normal = model.geometryData.updateNormals.front();
		EXPECT_TRUE(std::isfinite(normal.x));
		EXPECT_TRUE(std::isfinite(normal.y));
		EXPECT_TRUE(std::isfinite(normal.z));
		EXPECT_EQ(normal, glm::vec3(0));
	}

	TEST_F(AnimationModelContractTest, KeepsDegenerateIkDirectionsFinite) {
		for (const bool oppositeDirections : {false, true}) {
			const auto ikNode = std::make_shared<Node>();
			const auto targetNode = std::make_shared<Node>();
			const auto chainNode = std::make_shared<Node>();
			if (oppositeDirections) {
				ikNode->global = glm::translate(glm::mat4(1), glm::vec3(-1, 0, 0));
				targetNode->global = glm::translate(glm::mat4(1), glm::vec3(1, 0, 0));
			}
			IkSolver solver;
			solver.ikNode = ikNode;
			solver.ikTarget = targetNode;
			solver.limitAngle = glm::pi<float>();
			auto& chain = solver.chains.emplace_back();
			chain.node = chainNode;
			chain.saveIkRot = glm::quat(1, 0, 0, 0);
			solver.Solve();
			EXPECT_TRUE(IsFinite(chainNode->ikRotate));
		}
	}

	TEST_F(AnimationModelContractTest, UpdatesDeepChildTransformsWithoutTraversalStorage) {
		const auto root = std::make_shared<Node>();
		const auto child = std::make_shared<Node>();
		const auto grandchild = std::make_shared<Node>();
		root->AddChild(child);
		child->AddChild(grandchild);
		root->global = glm::translate(glm::mat4(1), glm::vec3(1, 0, 0));
		child->local = glm::translate(glm::mat4(1), glm::vec3(0, 2, 0));
		grandchild->local = glm::translate(glm::mat4(1), glm::vec3(0, 0, 3));
		root->UpdateChildTransform();
		EXPECT_EQ(glm::vec3(child->global[3]), glm::vec3(1, 2, 0));
		EXPECT_EQ(glm::vec3(grandchild->global[3]), glm::vec3(1, 2, 3));
	}
}
