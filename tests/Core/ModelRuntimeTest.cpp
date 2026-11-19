#include "Core/Model/Model.h"
#include "Core/Model/ModelUpdater.h"
#include "Core/Model/Physics/ScenePhysics.h"

#include <gtest/gtest.h>

namespace Chrivent {
	// 스키닝 형식과 프레임 갱신 계약에 필요한 최소 모델 구성을 공유한다.
	class ModelRuntimeContractTest : public testing::Test {
	protected:
		static void InitializeSkinningModel(Model& model, const WeightType weightType) {
			model.geometryData.positions.emplace_back(0.0f);
			model.geometryData.normals.emplace_back(0.0f, 1.0f, 0.0f);
			model.geometryData.uvs.emplace_back(0.0f);
			model.geometryData.updatePositions.resize(1);
			model.geometryData.updateNormals.resize(1);
			model.geometryData.updateUVs.resize(1);
			model.morphData.morphPositions.resize(1);
			model.morphData.morphUVs.resize(1);
			model.geometryData.vertexBoneInfos.emplace_back().weightType = weightType;
		}

		// 모델 갱신 계약을 통해 지정한 이동 행렬을 만드는 테스트 본을 추가한다.
		static void AddSkinningTransform(Model& model, const glm::vec3& translation) {
			auto node = std::make_unique<Node>();
			node->initTranslate = translation;
			model.skeletonData.AddNode(std::move(node));
		}

		// 공개된 모델 갱신 계약을 통해 첫 번째 스키닝 작업을 수행한다.
		static void RunSkinning(Model& model) {
			ModelUpdater::PrepareBeforePhysics(model, nullptr, 0.0f);
			ModelUpdater::PrepareAfterPhysics(model, false, false);
			ASSERT_EQ(ModelUpdater::CalculateSkinningTaskCount(model), 1);
			ModelUpdater::UpdateSkinning(model, 0);
		}
	};

	TEST_F(ModelRuntimeContractTest, AppliesTwoBoneLinearSkinningWeights) {
		Model model;
		InitializeSkinningModel(model, WeightType::BoneDeform2);
		auto& vertex = model.geometryData.vertexBoneInfos.front();
		vertex.boneIndices[0] = 0;
		vertex.boneIndices[1] = 1;
		vertex.boneWeights[0] = 0.25f;
		vertex.boneWeights[1] = 0.75f;
		AddSkinningTransform(model, glm::vec3(2, 0, 0));
		AddSkinningTransform(model, glm::vec3(4, 0, 0));
		RunSkinning(model);
		EXPECT_NEAR(model.geometryData.updatePositions.front().x, 3.5f, 1.0e-5f);
	}

	TEST_F(ModelRuntimeContractTest, AppliesFourBoneLinearSkinningWeights) {
		Model model;
		InitializeSkinningModel(model, WeightType::BoneDeform4);
		auto& vertex = model.geometryData.vertexBoneInfos.front();
		for (int index = 0; index < 4; index++) {
			vertex.boneIndices[index] = index;
			vertex.boneWeights[index] = static_cast<float>(index + 1) * 0.1f;
			AddSkinningTransform(model, glm::vec3(static_cast<float>(index + 1), 0, 0));
		}
		RunSkinning(model);
		EXPECT_NEAR(model.geometryData.updatePositions.front().x, 3.0f, 1.0e-5f);
	}

	TEST_F(ModelRuntimeContractTest, KeepsSphericalDeformSkinningFinite) {
		Model model;
		InitializeSkinningModel(model, WeightType::SphericalDeform);
		auto& vertex = model.geometryData.vertexBoneInfos.front();
		vertex.boneIndices[0] = 0;
		vertex.boneIndices[1] = 1;
		vertex.boneWeights[0] = 0.5f;
		vertex.sphericalDeformR0 = glm::vec3(1, 0, 0);
		vertex.sphericalDeformR1 = glm::vec3(1, 0, 0);
		for (int index = 0; index < 2; index++) {
			AddSkinningTransform(model, glm::vec3(static_cast<float>(index * 2), 0, 0));
		}
		RunSkinning(model);
		EXPECT_NEAR(model.geometryData.updatePositions.front().x, 2.0f, 1.0e-5f);
		EXPECT_TRUE(std::isfinite(model.geometryData.updateNormals.front().x));
	}

	TEST_F(ModelRuntimeContractTest, AppliesSingleInfluenceQuaternionSkinning) {
		Model model;
		InitializeSkinningModel(model, WeightType::QuaternionDeform);
		auto& vertex = model.geometryData.vertexBoneInfos.front();
		vertex.boneIndices[0] = 0;
		vertex.boneWeights[0] = 1.0f;
		for (int index = 1; index < 4; index++) {
			vertex.boneIndices[index] = -1;
			vertex.boneWeights[index] = 0.0f;
		}
		AddSkinningTransform(model, glm::vec3(2, 0, 0));
		RunSkinning(model);
		EXPECT_NEAR(model.geometryData.updatePositions.front().x, 2.0f, 1.0e-5f);
	}

	TEST_F(ModelRuntimeContractTest, PreservesPreviousPositionsThroughExplicitUpdateOptions) {
		Model model;
		InitializeSkinningModel(model, WeightType::BoneDeform1);
		auto& vertex = model.geometryData.vertexBoneInfos.front();
		vertex.boneIndices[0] = 0;
		vertex.boneWeights[0] = 1.0f;
		model.geometryData.updatePositions.front() = glm::vec3(7, 0, 0);
		model.skeletonData.AddNode(std::make_unique<Node>());
		ModelUpdater::PrepareBeforePhysics(model, nullptr, 0.0f);
		ModelUpdater::PrepareAfterPhysics(model, false, true);
		ASSERT_EQ(model.geometryData.previousPositions.size(), 1);
		EXPECT_EQ(model.geometryData.previousPositions.front(), glm::vec3(7, 0, 0));
	}

	TEST_F(ModelRuntimeContractTest, AddingNodeMaintainsThePoseLayout) {
		Model model;
		auto node = std::make_unique<Node>();
		Node* const nodeReference = node.get();
		model.skeletonData.AddNode(std::move(node));
		ASSERT_EQ(model.skeletonData.GetNodes().size(), 1);
		ASSERT_EQ(model.skeletonData.transforms.size(), 1);
		ASSERT_EQ(model.skeletonData.sortedNodes.size(), 1);
		EXPECT_EQ(model.skeletonData.GetNodes().front().get(), nodeReference);
		EXPECT_EQ(&model.skeletonData.sortedNodes.front().get(), nodeReference);
		EXPECT_EQ(model.skeletonData.transforms.front(), glm::mat4(1));
	}

	TEST_F(ModelRuntimeContractTest, UpdatesBeforeAndAfterPhysicsPoseStages) {
		Model model;
		auto beforePhysicsNode = std::make_unique<Node>();
		Node* const beforePhysicsNodeReference = beforePhysicsNode.get();
		beforePhysicsNode->initTranslate = glm::vec3(1, 0, 0);
		model.skeletonData.AddNode(std::move(beforePhysicsNode));
		auto afterPhysicsNode = std::make_unique<Node>();
		Node* const afterPhysicsNodeReference = afterPhysicsNode.get();
		afterPhysicsNode->initTranslate = glm::vec3(0, 2, 0);
		afterPhysicsNode->isDeformAfterPhysics = true;
		model.skeletonData.AddNode(std::move(afterPhysicsNode));
		ModelUpdater::PrepareBeforePhysics(model, nullptr, 0.0f);
		ModelUpdater::PrepareAfterPhysics(model, false, false);
		EXPECT_EQ(glm::vec3(beforePhysicsNodeReference->global[3]), glm::vec3(1, 0, 0));
		EXPECT_EQ(glm::vec3(afterPhysicsNodeReference->global[3]), glm::vec3(0, 2, 0));
		EXPECT_EQ(glm::vec3(model.skeletonData.transforms[0][3]), glm::vec3(1, 0, 0));
		EXPECT_EQ(glm::vec3(model.skeletonData.transforms[1][3]), glm::vec3(0, 2, 0));
	}

	TEST_F(ModelRuntimeContractTest, MixedStageChildReachesSkinningOnTheSameEvaluation) {
		Model model;
		InitializeSkinningModel(model, WeightType::BoneDeform1);
		model.geometryData.vertexBoneInfos.front().boneIndices[0] = 1;
		AddSkinningTransform(model, glm::vec3(3, 0, 0));
		AddSkinningTransform(model, glm::vec3(0));
		const auto& nodes = model.skeletonData.GetNodes();
		nodes[0]->AddChild(*nodes[1]);
		nodes[1]->isDeformAfterPhysics = true;
		for (const float y : {1.0f, 2.0f, -1.0f, -1.0f}) {
			nodes[1]->initTranslate.y = y;
			RunSkinning(model);
			const glm::vec3 expected(3, y, 0);
			EXPECT_EQ(glm::vec3(nodes[1]->global[3]), expected);
			EXPECT_EQ(glm::vec3(model.skeletonData.transforms[1][3]), expected);
			EXPECT_EQ(model.geometryData.updatePositions.front(), expected);
		}
	}

	TEST_F(ModelRuntimeContractTest, StepsAt60HzAndDiscardsFractionalTimeOnSeek) {
		ScenePhysics scene;
		Model first;
		Model second;
		constexpr RigidBodyDefinition body{.shapeSize = glm::vec3(1)};
		ASSERT_TRUE(first.InitializePhysics(scene, {body}, {}));
		ASSERT_TRUE(second.InitializePhysics(scene, {body}, {}));
		constexpr float halfTick = 1.0f / 120.0f;
		EXPECT_EQ(scene.Step(halfTick), 0);
		scene.Reset();
		EXPECT_EQ(scene.Step(halfTick), 0);
		EXPECT_EQ(scene.Step(halfTick), 1);
		EXPECT_EQ(scene.Step(1.0f / 30.0f), 2);
		EXPECT_EQ(scene.Step(1.0f), 10);
		scene.Reset();
		EXPECT_EQ(scene.Step(10.75f / 60.0f), 10);
		EXPECT_EQ(scene.Step(halfTick), 1);
	}

	TEST_F(ModelRuntimeContractTest, LoadingOrDiscardingAnotherSceneDoesNotAdvanceTheLiveClock) {
		ScenePhysics live;
		Model actor;
		constexpr RigidBodyDefinition body{.shapeSize = glm::vec3(1)};
		ASSERT_TRUE(actor.InitializePhysics(live, {body}, {}));
		EXPECT_EQ(live.Step(1.0f / 120.0f), 0);
		{
			ScenePhysics candidate;
			Model other;
			ASSERT_TRUE(other.InitializePhysics(candidate, {body}, {}));
			EXPECT_EQ(candidate.Step(1.0f / 30.0f), 2);
			candidate.Reset();
		}
		EXPECT_EQ(live.Step(1.0f / 120.0f), 1);
	}

	TEST_F(ModelRuntimeContractTest, ModelsCollideAtSceneScaleAndUnregisterIndependently) {
		ScenePhysics scene;
		Model platform;
		Model actor;
		constexpr RigidBodyDefinition box{.shape = RigidBodyShape::Box, .shapeSize = glm::vec3(2, 0.5f, 2),
			.translate = glm::vec3(0, 4, 0), .group = 1, .groupMask = 1};
		ASSERT_TRUE(platform.InitializePhysics(scene, {box}, {}, 2.0f));
		AddSkinningTransform(actor, glm::vec3(0, 13, 0));
		const auto& bone = actor.skeletonData.GetNodes().front();
		bone->isDeformAfterPhysics = true;
		ModelUpdater::InitializeAnimation(actor);
		constexpr RigidBodyDefinition ball{.nodeIndex = 0, .shapeSize = glm::vec3(1), .translate = glm::vec3(0, 13, 0),
			.mass = 1, .operation = RigidBodyOperation::Dynamic, .group = 0, .groupMask = 2};
		ASSERT_TRUE(actor.InitializePhysics(scene, {ball}, {}));
		const ModelAnimationTarget targets[]{{platform, nullptr}, {actor, nullptr}};
		ModelUpdater::ResetPhysicsAtFrame(scene, targets, 0);
		EXPECT_NEAR(bone->global[3].y, 10.0f, 0.05f);
		ModelUpdater::ResetPhysicsAtFrame(scene, targets, 1230.5f);
		EXPECT_NEAR(bone->global[3].y, 10.0f, 0.05f);
		platform.Reset();
		for (int tick = 0; tick < 120; ++tick) {
			ModelUpdater::PrepareBeforePhysics(actor, nullptr, 0);
			ASSERT_EQ(scene.Step(1.0f / 60.0f), 1);
			ModelUpdater::PrepareAfterPhysics(actor, true, false);
		}
		EXPECT_NEAR(bone->global[3].y, 1.0f, 0.05f);
		EXPECT_NEAR(actor.skeletonData.transforms[0][3].y, 1.0f, 0.05f);
	}

	TEST_F(ModelRuntimeContractTest, ReflectsParentBeforeChildRegardlessOfBodyFileOrder) {
		ScenePhysics scene;
		Model model;
		AddSkinningTransform(model, glm::vec3(0, 20, 0));
		AddSkinningTransform(model, glm::vec3(0, 3, 0));
		const auto& nodes = model.skeletonData.GetNodes();
		nodes[0]->AddChild(*nodes[1]);
		nodes[1]->isDeformAfterPhysics = true;
		ModelUpdater::InitializeAnimation(model);
		constexpr RigidBodyDefinition child{.nodeIndex = 1, .shapeSize = glm::vec3(0.5f), .translate = glm::vec3(0, 23, 0),
			.mass = 1, .operation = RigidBodyOperation::Dynamic};
		auto parent = child;
		parent.nodeIndex = 0;
		parent.translate.y = 20;
		ASSERT_TRUE(model.InitializePhysics(scene, {child, parent}, {}));
		for (int tick = 0; tick < 15; ++tick) {
			ModelUpdater::PrepareBeforePhysics(model, nullptr, 0);
			ASSERT_EQ(scene.Step(1.0f / 60.0f), 1);
			ModelUpdater::PrepareAfterPhysics(model, true, false);
			EXPECT_NEAR(nodes[1]->global[3].y - nodes[0]->global[3].y, 3.0f, 0.001f);
		}
		EXPECT_LT(nodes[0]->global[3].y, 19.0f);
	}
}
