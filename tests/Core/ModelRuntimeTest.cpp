#include "Core/Model/Model.h"
#include "Core/Model/ModelUpdater.h"

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
			ModelUpdater::Prepare(model, {.updatePhysics = false});
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
		ModelUpdater::Prepare(model, {
			.preservePreviousPositions = true,
			.updatePhysics = false
		});
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
		ModelUpdater::Prepare(model, {.updatePhysics = false});
		EXPECT_EQ(glm::vec3(beforePhysicsNodeReference->global[3]), glm::vec3(1, 0, 0));
		EXPECT_EQ(glm::vec3(afterPhysicsNodeReference->global[3]), glm::vec3(0, 2, 0));
		EXPECT_EQ(glm::vec3(model.skeletonData.transforms[0][3]), glm::vec3(1, 0, 0));
		EXPECT_EQ(glm::vec3(model.skeletonData.transforms[1][3]), glm::vec3(0, 2, 0));
	}
}
