#include "Core/Model/Model.h"
#include "Core/Model/ModelSkinning.h"
#include "Core/Model/ModelUpdater.h"

#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>

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

		static void RunSkinning(Model& model) {
			ModelSkinning::PrepareUpdate(model, false);
			ASSERT_EQ(ModelSkinning::GetUpdateRangeCount(model), 1);
			ModelSkinning::UpdateRange(model, 0);
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
		model.skeletonData.transforms.emplace_back(glm::translate(glm::mat4(1), glm::vec3(2, 0, 0)));
		model.skeletonData.transforms.emplace_back(glm::translate(glm::mat4(1), glm::vec3(4, 0, 0)));
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
			model.skeletonData.transforms.emplace_back(
				glm::translate(glm::mat4(1), glm::vec3(static_cast<float>(index + 1), 0, 0)));
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
			auto node = std::make_shared<Node>();
			node->global = glm::mat4(1);
			model.skeletonData.AddNode(std::move(node));
			model.skeletonData.transforms.emplace_back(
				glm::translate(glm::mat4(1), glm::vec3(static_cast<float>(index * 2), 0, 0)));
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
		model.skeletonData.transforms.emplace_back(glm::translate(glm::mat4(1), glm::vec3(2, 0, 0)));
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
		const auto node = std::make_shared<Node>();
		model.skeletonData.AddNode(node);
		model.skeletonData.sortedNodes.emplace_back(*node);
		model.skeletonData.transforms.emplace_back(1.0f);
		ModelUpdater::Prepare(model, {
			.preservePreviousPositions = true,
			.updatePhysics = false
		});
		ASSERT_EQ(model.geometryData.previousPositions.size(), 1);
		EXPECT_EQ(model.geometryData.previousPositions.front(), glm::vec3(7, 0, 0));
	}
}
