#include "Core/Animation/Camera/CameraAnimation.h"
#include "Core/Model/Model.h"
#include "Core/Model/ModelLoader.h"
#include "Core/Model/ModelSkinning.h"

#include <gtest/gtest.h>

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
		const ModelSkinning skinning(model);
		skinning.PrepareUpdate(false);
		ASSERT_EQ(skinning.GetUpdateRangeCount(), 1);
		EXPECT_EQ(model.geometryData.updateRanges.front().vertexOffset, 0);
		EXPECT_EQ(model.geometryData.updateRanges.front().vertexCount, 10);
		model.geometryData.positions.clear();
		skinning.PrepareUpdate(false);
		EXPECT_EQ(skinning.GetUpdateRangeCount(), 0);
	}

	TEST_F(AnimationModelContractTest, FailedLoadPreservesTheExistingModel) {
		Model model;
		model.infoData.modelName = "기존 모델";
		const ModelLoader loader(model);
		const auto result = loader.Load(
			"pmxmod_core_test_file_that_does_not_exist.pmx", {});
		ASSERT_FALSE(result);
		EXPECT_EQ(result.error().code, ModelLoadErrorCode::Parse);
		EXPECT_EQ(model.infoData.modelName, "기존 모델");
	}
}
