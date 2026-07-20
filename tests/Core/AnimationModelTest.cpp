#include "Core/Animation/Camera/CameraAnimation.h"
#include "Core/Animation/Model/AnimationBuilder.h"
#include "Core/Model/Model.h"
#include "Core/Model/ModelLoader.h"
#include "Core/Model/ModelSkinning.h"

#include <gtest/gtest.h>
#include <fstream>
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

	TEST_F(AnimationModelContractTest, LoadsAnEmptyModelWithAZeroBoundingBox) {
		const auto path = std::filesystem::temp_directory_path() / "pmxmod_empty_model_test.pmx";
		const std::string bytes = BuildMinimalPmx();
		{
			std::ofstream file(path, std::ios::binary);
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}
		Model model;
		const ModelLoader loader(model);
		const auto result = loader.Load(path, {});
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
		model->skeletonData.nodes.emplace_back(node);
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

	TEST_F(AnimationModelContractTest, KeepsZeroNormalsFiniteDuringSkinning) {
		Model model;
		model.geometryData.positions.emplace_back(0);
		model.geometryData.normals.emplace_back(0);
		model.geometryData.uvs.emplace_back(0);
		auto& vertex = model.geometryData.vertexBoneInfos.emplace_back();
		vertex.weightType = WeightType::BoneDeform1;
		vertex.boneIndices[0] = 0;
		vertex.boneWeights[0] = 1.0f;
		model.geometryData.updatePositions.resize(1);
		model.geometryData.updateNormals.resize(1);
		model.geometryData.updateUVs.resize(1);
		model.morphData.morphPositions.resize(1);
		model.morphData.morphUVs.resize(1);
		model.skeletonData.transforms.emplace_back(1);
		const ModelSkinning skinning(model);
		skinning.PrepareUpdate(false);
		skinning.UpdateRange(0);
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
}
