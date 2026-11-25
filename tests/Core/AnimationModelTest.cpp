#include "Core/Animation/Camera/CameraAnimation.h"
#include "Core/Animation/Camera/CameraAnimationBuilder.h"
#include "Core/Animation/Model/AnimationBuilder.h"
#include "Core/Model/Model.h"
#include "Core/Model/ModelLoader.h"
#include "Core/Model/ModelUpdater.h"
#include "Core/Model/Physics/ScenePhysics.h"
#include "Core/Parser/PmxParser.h"

#include <gtest/gtest.h>
#include <fstream>
#include <limits>
#include <utility>
#include <glm/gtc/matrix_transform.hpp>

namespace Chrivent {
	// 카메라 키 생성과 모델 상태 검증을 공유한다.
	class AnimationModelContractTest : public testing::Test {
	protected:
		ScenePhysics scene;

		static void PreparePose(Model& model) {
			ModelUpdater::PrepareBeforePhysics(model, nullptr, 0.0f);
			ModelUpdater::PrepareAfterPhysics(model, false, false);
		}

		// 테스트 모델에 노드를 추가하고 모델 수명 동안 사용할 관찰 포인터를 반환한다.
		static Node* AddNode(Model& model, std::string name = {}) {
			auto node = std::make_unique<Node>();
			node->name = std::move(name);
			Node* result = node.get();
			model.skeletonData.AddNode(std::move(node));
			return result;
		}

		// 테스트 모델에 IK 솔버를 추가하고 모델 수명 동안 사용할 관찰 포인터를 반환한다.
		static IkSolver* AddIkSolver(Model& model) {
			auto ikSolver = std::make_unique<IkSolver>();
			IkSolver* result = ikSolver.get();
			model.skeletonData.AddIkSolver(std::move(ikSolver));
			return result;
		}

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

		// 정점, 면, 재질과 본 변환을 함께 검증할 수 있는 최소 PMX를 만든다.
		static std::string BuildRenderablePmx(const bool includeUvMorphs = false) {
			std::string bytes;
			bytes.append("PMX ", 4);
			Append(bytes, 2.0f);
			const uint8_t headerData[] = {8, 1, static_cast<uint8_t>(includeUvMorphs), 1, 1, 1, 1, 1, 1};
			bytes.append(reinterpret_cast<const char*>(headerData), sizeof(headerData));
			const auto AppendString = [&](const std::string& value) {
				const int32_t length = static_cast<int32_t>(value.size());
				Append(bytes, length);
				bytes.append(value);
			};
			AppendString("Triangle");
			AppendString({});
			AppendString({});
			AppendString({});
			constexpr int32_t vertexCount = 3;
			Append(bytes, vertexCount);
			const auto AppendVertex = [&](const glm::vec3& position, const glm::vec2& uv) {
				Append(bytes, position);
				Append(bytes, glm::vec3(0, 0, 1));
				Append(bytes, uv);
				if (includeUvMorphs)
					Append(bytes, glm::vec4(1, 2, 3, 4));
				Append(bytes, PmxParser::WeightType::BoneDeform1);
				Append(bytes, uint8_t{0});
				Append(bytes, includeUvMorphs ? 0.0f : 1.0f);
			};
			AppendVertex(glm::vec3(-1, 0, 2), glm::vec2(0, 0));
			AppendVertex(glm::vec3(1, 0, 2), glm::vec2(1, 0));
			AppendVertex(glm::vec3(0, 2, 2), glm::vec2(0.5f, 1));
			constexpr int32_t indexCount = 3;
			Append(bytes, indexCount);
			Append(bytes, uint8_t{0});
			Append(bytes, uint8_t{1});
			Append(bytes, uint8_t{2});
			constexpr int32_t emptyCount = 0;
			Append(bytes, emptyCount);
			constexpr int32_t materialCount = 1;
			Append(bytes, materialCount);
			AppendString("Material");
			AppendString({});
			Append(bytes, glm::vec4(0.8f, 0.7f, 0.6f, 1));
			Append(bytes, glm::vec3(0.1f));
			Append(bytes, 8.0f);
			Append(bytes, glm::vec3(0.2f));
			Append(bytes, PmxParser::DrawModeFlags::DrawEdge);
			Append(bytes, glm::vec4(0, 0, 0, 1));
			Append(bytes, 1.0f);
			constexpr uint8_t missingIndex = 0xFF;
			Append(bytes, missingIndex);
			Append(bytes, missingIndex);
			Append(bytes, PmxParser::SphereMode::None);
			Append(bytes, PmxParser::ToonMode::Separate);
			Append(bytes, missingIndex);
			AppendString({});
			Append(bytes, indexCount);
			constexpr int32_t boneCount = 1;
			Append(bytes, boneCount);
			AppendString("Root");
			AppendString({});
			Append(bytes, glm::vec3(0));
			Append(bytes, missingIndex);
			Append(bytes, int32_t{0});
			Append(bytes, PmxParser::BoneFlags::Visible);
			Append(bytes, glm::vec3(0, 1, 0));
			Append(bytes, includeUvMorphs ? int32_t{5} : emptyCount);
			if (includeUvMorphs) {
				for (int channel = 0; channel <= 4; ++channel) {
					AppendString("UV" + std::to_string(channel));
					AppendString({});
					Append(bytes, uint8_t{1});
					Append(bytes, static_cast<uint8_t>(static_cast<int>(PmxParser::MorphType::Uv) + channel));
					Append(bytes, int32_t{1});
					Append(bytes, uint8_t{0});
					Append(bytes, glm::vec4(0.25f, 0.5f, 1, 2));
				}
			}
			for (int sectionIndex = 0; sectionIndex < 3; sectionIndex++)
				Append(bytes, emptyCount);
			return bytes;
		}

		static bool IsFinite(const glm::quat& value) {
			return std::isfinite(value.w) && std::isfinite(value.x) &&
				std::isfinite(value.y) && std::isfinite(value.z);
		}
	};

	TEST_F(AnimationModelContractTest, PreservesVertexEdgeScaleAndAllFiveUvMorphChannels) {
		const auto path = std::filesystem::temp_directory_path() / "pmxmod_uv_channels_test.pmx";
		const auto bytes = BuildRenderablePmx(true);
		{
			std::ofstream file(path, std::ios::binary);
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}
		Model model;
		const auto result = ModelLoader::Load(model, path, {}, scene);
		std::filesystem::remove(path);
		ASSERT_TRUE(result);
		EXPECT_FLOAT_EQ(model.geometryData.edgeScales.at(0), 0.0f);
		for (const auto& morph : model.morphData.GetMorphs())
			morph->weight = 1;
		for (int frame = 0; frame < 2; ++frame) {
			PreparePose(model);
			ModelUpdater::UpdateSkinning(model, 0);
			EXPECT_EQ(model.geometryData.updateUVs.at(0), glm::vec2(0.25f, 0.5f));
			EXPECT_EQ(model.geometryData.updateAdditionalUVs[0].at(0), glm::vec4(1.25f, 2.5f, 4, 6));
			for (size_t channel = 1; channel < 4; ++channel)
				EXPECT_EQ(model.geometryData.updateAdditionalUVs[channel].at(0), glm::vec4(0.25f, 0.5f, 1, 2));
		}
	}

	TEST_F(AnimationModelContractTest, TwoOneSidedIkAxesAreNotASingleHinge) {
		Node joint, tip, goal;
		joint.enableIk = true;
		joint.AddChild(tip);
		tip.translate = glm::vec3(1, 0, 0);
		goal.translate = glm::vec3(0, 1, 0);
		tip.UpdateLocalTransform();
		joint.UpdateGlobalTransform();
		goal.UpdateLocalTransform();
		goal.UpdateGlobalTransform();
		IkSolver solver;
		solver.ikNode = &goal;
		solver.ikTarget = &tip;
		solver.iterateCount = 4;
		solver.chains.push_back({ &joint, true, glm::vec3(0, glm::half_pi<float>(), glm::pi<float>()) });
		solver.Solve();
		EXPECT_LT(glm::distance(glm::vec3(tip.global[3]), goal.translate), 0.001f);
	}

	TEST_F(AnimationModelContractTest, LocalGrantUsesTheFinalLocalTransformOnlyOnce) {
		Node source, middle, child;
		source.animTranslate.x = 1;
		source.animRotate = glm::angleAxis(0.4f, glm::vec3(0, 0, 1));
		source.UpdateLocalTransform();
		middle.appendNode = &source;
		middle.isAppendRotate = middle.isAppendTranslate = true;
		middle.appendWeight = 1;
		middle.initTranslate.x = middle.translate.x = 10;
		middle.animTranslate.x = 2;
		middle.scale = glm::vec3(2, 3, 4);
		middle.UpdateAppendTransform();
		child.appendNode = &middle;
		child.isAppendLocal = child.isAppendTranslate = child.isAppendRotate = true;
		child.appendWeight = 1;
		child.UpdateAppendTransform();
		EXPECT_NEAR(child.local[3].x, 3.0f, 0.0001f);
		EXPECT_NEAR(glm::abs(glm::dot(glm::quat_cast(child.local), source.animRotate)), 1.0f, 0.0001f);
	}

	TEST_F(AnimationModelContractTest, VmdIkSwitchesOnItsRecordedFrames) {
		const auto model = std::make_shared<Model>();
		auto* const solver = AddIkSolver(*model);
		solver->ikNode = AddNode(*model, "LegIK");
		VmdParser::VmdData data{};
		data.iks = {{0, 1, {{ "LegIK", 1 }}}, {6, 1, {{"LegIK", 0}}}, {12, 1, {{"LegIK", 1}}}};
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		animation->Evaluate(5.5f);
		EXPECT_TRUE(solver->enable);
		animation->Evaluate(6);
		EXPECT_FALSE(solver->enable);
		animation->Evaluate(11.5f);
		EXPECT_FALSE(solver->enable);
		animation->Evaluate(12);
		EXPECT_TRUE(solver->enable);
	}

	TEST_F(AnimationModelContractTest, VmdVisibilityAndBonePhysicsAreIndependentStepKeys) {
		auto model = std::make_shared<Model>();
		const auto* node = AddNode(*model, "Root");
		VmdParser::VmdData data{};
		VmdParser::VmdMotion key{};
		std::ranges::copy(std::string_view("Root"), key.boneName);
		key.quaternion = glm::quat(1, 0, 0, 0);
		key.frame = 4;
		key.interpolation[2] = 99;
		key.interpolation[3] = 15;
		data.motions.push_back(key);
		key.frame = 7;
		key.interpolation[17] = 99;
		key.interpolation[18] = 15;
		data.motions.push_back(key);
		data.iks = {{3, 0, {}}, {12, 1, {}}};
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		EXPECT_EQ(animation->CalculateLastFrame(), 12u);
		animation->Evaluate(6.5f);
		EXPECT_FALSE(node->physicsEnabled);
		EXPECT_FALSE(model->visible);
		animation->Evaluate(7);
		EXPECT_TRUE(node->physicsEnabled);
		EXPECT_FALSE(model->visible);
		animation->Evaluate(12);
		EXPECT_TRUE(model->visible);
		animation->Evaluate(2);
		EXPECT_TRUE(node->physicsEnabled);
		EXPECT_TRUE(model->visible);
	}

	TEST_F(AnimationModelContractTest, DisabledPhysicsFollowsTheBoneAndResumesFromItsCurrentPose) {
		auto model = std::make_shared<Model>();
		auto* const bone = AddNode(*model, "Root");
		bone->initTranslate.y = 15;
		ModelUpdater::InitializeAnimation(*model);
		constexpr auto body = [] {
			RigidBodyDefinition value{};
			value.nodeIndex = 0;
			value.shapeSize = glm::vec3(0.25f);
			value.translate = glm::vec3(0, 15, 0);
			value.mass = 1;
			value.operation = RigidBodyOperation::Dynamic;
			return value;
		}();
		ASSERT_TRUE(model->InitializePhysics(scene, {body}, {}));
		VmdParser::VmdData data{};
		VmdParser::VmdMotion key{};
		std::ranges::copy(std::string_view("Root"), key.boneName);
		key.quaternion = glm::quat(1, 0, 0, 0);
		key.interpolation[2] = 99;
		key.interpolation[3] = 15;
		data.motions.push_back(key);
		key.frame = 4;
		key.translate.x = 2;
		data.motions.push_back(key);
		key.frame = 8;
		key.interpolation[2] = key.interpolation[3] = 0;
		data.motions.push_back(key);
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		const auto Tick = [&](const float frame) {
			ModelUpdater::PrepareBeforePhysics(*model, animation.get(), frame);
			EXPECT_EQ(scene.Step(1.0f / 30.0f), 2);
			ModelUpdater::PrepareAfterPhysics(*model, true, false);
		};
		for (int frame = 0; frame < 8; ++frame)
			Tick(static_cast<float>(frame));
		EXPECT_NEAR(bone->global[3].x, 2, 0.001f);
		EXPECT_NEAR(bone->global[3].y, 15, 0.001f);
		for (int frame = 8; frame <= 19; ++frame)
			Tick(static_cast<float>(frame));
		EXPECT_LT(bone->global[3].y, 14);
		EXPECT_NEAR(bone->global[3].x, 2, 0.001f);
		Tick(0);
		EXPECT_NEAR(bone->global[3].x, 0, 0.001f);
		EXPECT_NEAR(bone->global[3].y, 15, 0.001f);
	}

	TEST_F(AnimationModelContractTest, CameraProjectionAndLightTracksHaveIndependentBoundaries) {
		const auto [defaultColor, defaultDirection] = CameraAnimationBuilder::Build({})->EvaluateLighting(0);
		EXPECT_EQ(defaultColor, glm::vec3(154.0f / 255.0f));
		EXPECT_EQ(defaultDirection, glm::vec3(-0.5f, -1, -0.5f));
		VmdParser::VmdData data{};
		VmdParser::VmdCamera camera{};
		camera.distance = -40;
		camera.viewAngle = 30;
		data.cameras.push_back(camera);
		camera.frame = 10;
		camera.isPerspective = 1;
		data.cameras.push_back(camera);
		data.lights = {{0, glm::vec3(0), {0, -1, -1}}, {20, glm::vec3(1), {1, -1, -1}}};
		data.shadows = {{100, VmdParser::ShadowType::Mode1, 0.02f}}; // 사용하지 않는 키는 재생 길이를 늘리지 않는다.
		const auto animation = CameraAnimationBuilder::Build(data);
		EXPECT_EQ(animation->GetLastFrame(), 20u);
		EXPECT_TRUE(animation->Evaluate(9.9f).perspective);
		const auto ortho = animation->Evaluate(10);
		EXPECT_FALSE(ortho.perspective);
		const auto matrix = ortho.CalculateProjection(800, 600, 1, 100);
		EXPECT_FLOAT_EQ(matrix[3][3], 1);
		EXPECT_NEAR(matrix[1][1] / matrix[0][0], 800.0f / 600.0f, 0.0001f);
		const auto [interpolatedColor, interpolatedDirection] = animation->EvaluateLighting(10);
		EXPECT_EQ(interpolatedColor, glm::vec3(0.5f));
		EXPECT_EQ(interpolatedDirection, glm::vec3(0.5f, -1, 1));
		EXPECT_EQ(animation->EvaluateLighting(20).color, glm::vec3(1));
	}

	TEST_F(AnimationModelContractTest, CutsExactlyAtAdjacentCameraFrame) {
		std::vector<CameraAnimationKey> keys;
		keys.emplace_back(CreateCameraKey(0, 0.0f));
		keys.emplace_back(CreateCameraKey(1, 10.0f));
		const CameraAnimation animation(std::move(keys));
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
		const Node* node = AddNode(*model);
		std::vector<NodeAnimationKey> keys(3);
		keys[0].frame = 5;
		keys[0].translate = glm::vec3(1, 0, 0);
		keys[1].frame = 1;
		keys[1].translate = glm::vec3(0);
		keys[2].frame = 5;
		keys[2].translate = glm::vec3(2, 0, 0);
		std::vector<NodeAnimationTrack> tracks;
		tracks.push_back({0, std::move(keys)});
		const Animation animation(model, std::move(tracks), {}, {});
		ASSERT_EQ(animation.GetNodeTracks().front().keys.size(), 2);
		animation.Evaluate(5.0f);
		EXPECT_EQ(node->animTranslate, glm::vec3(2, 0, 0));
	}

	TEST_F(AnimationModelContractTest, EmptyTracksResetTheirTargets) {
		const auto model = std::make_shared<Model>();
		Node* node = AddNode(*model);
		IkSolver* ikSolver = AddIkSolver(*model);
		auto morph = std::make_unique<Morph>();
		Morph* const morphTarget = morph.get();
		model->morphData.AddMorph(std::move(morph));
		node->animTranslate = glm::vec3(3);
		node->animRotate = glm::quat(0, 1, 0, 0);
		ikSolver->enable = false;
		morphTarget->weight = 0.75f;
		const Animation animation(model,
			{{}},
			{{}},
			{{}});
		animation.Evaluate(0);
		EXPECT_EQ(node->animTranslate, glm::vec3(0));
		EXPECT_EQ(node->animRotate, glm::quat(1, 0, 0, 0));
		EXPECT_TRUE(ikSolver->enable);
		EXPECT_FLOAT_EQ(morphTarget->weight, 0);
	}

	TEST_F(AnimationModelContractTest, CalculatesLastFrameAcrossEveryTrackType) {
		const auto model = std::make_shared<Model>();
		AddNode(*model);
		AddIkSolver(*model);
		auto morph = std::make_unique<Morph>();
		model->morphData.AddMorph(std::move(morph));
		NodeAnimationKey nodeKey{};
		nodeKey.frame = 3;
		IkAnimationKey ikKey{};
		ikKey.frame = 7;
		MorphAnimationKey morphKey{};
		morphKey.frame = 5;
		const Animation animation(model,
			{{0, {nodeKey}}},
			{{0, {ikKey}}},
			{{0, {morphKey}}});
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
		PreparePose(model);
		ASSERT_EQ(ModelUpdater::CalculateSkinningTaskCount(model), 1);
		EXPECT_EQ(model.geometryData.updateRanges.front().vertexOffset, 0);
		EXPECT_EQ(model.geometryData.updateRanges.front().vertexCount, 10);
		model.geometryData.positions.clear();
		PreparePose(model);
		EXPECT_EQ(ModelUpdater::CalculateSkinningTaskCount(model), 0);
	}

	TEST_F(AnimationModelContractTest, FailedLoadPreservesTheExistingModel) {
		Model model;
		model.infoData.modelName = "기존 모델";
		const auto result = ModelLoader::Load(
			model, "pmxmod_core_test_file_that_does_not_exist.pmx", {}, scene);
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
		const auto result = ModelLoader::Load(model, path, {}, scene);
		std::error_code error;
		std::filesystem::remove(path, error);
		ASSERT_TRUE(result);
		EXPECT_EQ(model.geometryData.bboxMin, glm::vec3(0));
		EXPECT_EQ(model.geometryData.bboxMax, glm::vec3(0));
	}

	TEST_F(AnimationModelContractTest, LoadsRenderableModelDataThroughTheRuntimeBoundary) {
		const auto path = std::filesystem::temp_directory_path() / "pmxmod_renderable_model_test.pmx";
		const std::string bytes = BuildRenderablePmx();
		{
			std::ofstream file(path, std::ios::binary);
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}
		Model model;
		const auto result = ModelLoader::Load(model, path, {}, scene);
		std::error_code error;
		std::filesystem::remove(path, error);
		ASSERT_TRUE(result);
		EXPECT_EQ(model.infoData.modelName, "Triangle");
		ASSERT_EQ(model.geometryData.positions.size(), 3);
		EXPECT_EQ(model.geometryData.positions.front(), glm::vec3(-1, 0, -2));
		EXPECT_EQ(model.geometryData.bboxMin, glm::vec3(-1, 0, -2));
		EXPECT_EQ(model.geometryData.bboxMax, glm::vec3(1, 2, -2));
		EXPECT_EQ(model.geometryData.uvs.front(), glm::vec2(0, 1));
		ASSERT_EQ(model.geometryData.indices.size(), 3);
		EXPECT_EQ(model.geometryData.indices[0], char{2});
		EXPECT_EQ(model.geometryData.indices[1], char{1});
		EXPECT_EQ(model.geometryData.indices[2], char{0});
		ASSERT_EQ(model.materialData.materials.size(), 1);
		EXPECT_EQ(model.materialData.materials.front().edgeFlag, 1);
		ASSERT_EQ(model.materialData.subMeshes.size(), 1);
		EXPECT_EQ(model.materialData.subMeshes.front().indexCount, 3);
		ASSERT_EQ(model.skeletonData.GetNodes().size(), 1);
		EXPECT_EQ(model.skeletonData.GetNodes().front()->name, "Root");
		EXPECT_EQ(model.morphData.morphPositions.size(), 3);
		PreparePose(model);
		ASSERT_EQ(ModelUpdater::CalculateSkinningTaskCount(model), 1);
		ModelUpdater::UpdateSkinning(model, 0);
		EXPECT_EQ(model.geometryData.updatePositions.front(), glm::vec3(-1, 0, -2));
	}

	TEST_F(AnimationModelContractTest, NormalizesVmdBoneRotationsAtTheRuntimeBoundary) {
		auto model = std::make_shared<Model>();
		AddNode(*model, "A");
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
		const Node* nodeReference;
		std::unique_ptr<Animation> animation;
		{
			const auto model = std::make_shared<Model>();
			nodeReference = AddNode(*model, "A");
			modelReference = model;
			VmdParser::VmdData data{};
			auto& motion = data.motions.emplace_back();
			std::memcpy(motion.boneName, "A", 1);
			motion.quaternion = glm::quat(1, 0, 0, 0);
			AnimationBuilder builder(model);
			builder.Build(data);
			animation = builder.TakeAnimation();
		}
		ASSERT_FALSE(modelReference.expired());
		animation->Evaluate(0, 1);
		EXPECT_EQ(nodeReference->animTranslate, glm::vec3(0));
		animation.reset();
		EXPECT_TRUE(modelReference.expired());
	}

	TEST_F(AnimationModelContractTest, InvalidatesAnimationBindingsWhenModelStructureChanges) {
		const auto model = std::make_shared<Model>();
		AddNode(*model, "A");
		VmdParser::VmdData data{};
		auto& motion = data.motions.emplace_back();
		std::memcpy(motion.boneName, "A", 1);
		motion.translate = glm::vec3(3, 0, 0);
		motion.quaternion = glm::quat(1, 0, 0, 0);
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		EXPECT_TRUE(animation->IsBoundTo(*model));
		model->Reset();
		Node* replacementNode = AddNode(*model, "A");
		EXPECT_FALSE(animation->IsBoundTo(*model));
		replacementNode->animTranslate = glm::vec3(7);
		animation->Evaluate(0);
		EXPECT_EQ(replacementNode->animTranslate, glm::vec3(7));
	}

	TEST_F(AnimationModelContractTest, InvalidatesAnimationBindingsWhenOwnedStructureIsExchanged) {
		const auto model = std::make_shared<Model>();
		Node* originalNode = AddNode(*model);
		auto originalMorph = std::make_unique<Morph>();
		Morph* originalMorphReference = originalMorph.get();
		model->morphData.AddMorph(std::move(originalMorph));
		NodeAnimationKey nodeKey{};
		nodeKey.translate = glm::vec3(3, 0, 0);
		MorphAnimationKey morphKey{};
		morphKey.morphWeight = 1.0f;
		const Animation animation(model,
			{{ 0, {nodeKey} }},
			{},
			{{ 0, {morphKey} }});
		Model replacement;
		Node* replacementNode = AddNode(replacement);
		auto replacementMorph = std::make_unique<Morph>();
		Morph* replacementMorphReference = replacementMorph.get();
		replacement.morphData.AddMorph(std::move(replacementMorph));
		originalNode->animTranslate = glm::vec3(7, 0, 0);
		originalMorphReference->weight = 0.25f;
		replacementNode->animTranslate = glm::vec3(9, 0, 0);
		replacementMorphReference->weight = 0.5f;
		model->skeletonData.Swap(replacement.skeletonData);
		model->morphData.Swap(replacement.morphData);
		animation.Evaluate(0);
		EXPECT_EQ(originalNode->animTranslate, glm::vec3(7, 0, 0));
		EXPECT_FLOAT_EQ(originalMorphReference->weight, 0.25f);
		EXPECT_EQ(replacementNode->animTranslate, glm::vec3(9, 0, 0));
		EXPECT_FLOAT_EQ(replacementMorphReference->weight, 0.5f);
	}

	TEST_F(AnimationModelContractTest, BindsTracksToTheCurrentModelWhenTaken) {
		const auto model = std::make_shared<Model>();
		AddNode(*model, "A");
		VmdParser::VmdData data{};
		auto& motion = data.motions.emplace_back();
		std::memcpy(motion.boneName, "A", 1);
		motion.translate = glm::vec3(4, 0, 0);
		motion.quaternion = glm::quat(1, 0, 0, 0);
		AnimationBuilder builder(model);
		builder.Build(data);
		model->Reset();
		Node* replacementNode = AddNode(*model, "A");
		const auto animation = builder.TakeAnimation();
		ASSERT_EQ(animation->GetNodeTracks().size(), 1);
		EXPECT_EQ(animation->GetNodeTracks().front().targetIndex, 0);
		EXPECT_TRUE(animation->IsBoundTo(*model));
		animation->Evaluate(0);
		EXPECT_EQ(replacementNode->animTranslate, glm::vec3(4, 0, 0));
	}

	TEST_F(AnimationModelContractTest, KeepsTheLastModelKeyAtADuplicateFrame) {
		const auto model = std::make_shared<Model>();
		AddNode(*model, "A");
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
		AddNode(*model, "A");
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

	TEST_F(AnimationModelContractTest, ReadsIndependentBoneCurvesAndIgnoresFirstBlockFlags) {
		const auto model = std::make_shared<Model>();
		const Node* node = AddNode(*model, "IK");
		VmdParser::VmdData data{};
		for (const uint32_t frame : {1230u, 1231u}) {
			auto& key = data.motions.emplace_back();
			std::memcpy(key.boneName, "IK", 2);
			key.frame = frame;
			key.quaternion = glm::quat(1, 0, 0, 0);
			key.translate = glm::vec3(0);
		}
		auto& destination = data.motions.back();
		destination.translate = glm::vec3(8);
		destination.quaternion = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 0, 1));
		destination.interpolation[2] = 0x63;
		destination.interpolation[3] = 0x0f;
		for (std::size_t channel = 0; channel < 4; ++channel) {
			constexpr uint8_t curves[][4]{{0, 0, 127, 0}, {0, 127, 127, 127}, {20, 20, 107, 107}, {35, 35, 80, 80}};
			for (std::size_t point = 0; point < 4; ++point)
				destination.interpolation[16 * channel + 4 * point] = curves[channel][point];
		}
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		animation->Evaluate(1230.5f);
		EXPECT_EQ(node->animTranslate, glm::vec3(1, 7, -4));
		EXPECT_NEAR(glm::degrees(glm::angle(node->animRotate)), 45.0f, 0.001f);
	}

	TEST_F(AnimationModelContractTest, ResetsModelStateThroughTheUpdaterContract) {
		const auto model = std::make_shared<Model>();
		Node* node = AddNode(*model, "A");
		VmdParser::VmdData data{};
		auto& motion = data.motions.emplace_back();
		std::memcpy(motion.boneName, "A", 1);
		motion.frame = 12;
		motion.translate = glm::vec3(3, 0, 0);
		motion.quaternion = glm::quat(1, 0, 0, 0);
		AnimationBuilder builder(model);
		builder.Build(data);
		const auto animation = builder.TakeAnimation();
		const ModelAnimationTarget targets[]{{ *model, animation.get() }};
		ModelUpdater::ResetPhysicsAtFrame(scene, targets, 12.0f);
		EXPECT_EQ(node->animTranslate, glm::vec3(3, 0, 0));
		EXPECT_EQ(glm::vec3(node->global[3]), glm::vec3(3, 0, 0));
		EXPECT_EQ(glm::vec3(model->skeletonData.transforms[0][3]), glm::vec3(3, 0, 0));
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
		AddNode(*model, "A");
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
		Node* node = AddNode(*model);
		node->animTranslate = glm::vec3(8);
		node->baseAnimTranslate = glm::vec3(5);
		const Animation animation(model, {}, {}, {});
		const ModelAnimationTarget targets[]{{*model, &animation}};
		ModelUpdater::ResetPhysicsAtFrame(scene, targets, 0);
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

	TEST_F(AnimationModelContractTest, UnregistersModelBodiesFromTheSceneOnReset) {
		Model model;
		RigidBodyDefinition rigidBody{};
		rigidBody.shapeSize = glm::vec3(1);
		rigidBody.groupMask = 0xffff;
		ASSERT_TRUE(model.InitializePhysics(scene, {rigidBody}, {}));
		ASSERT_TRUE(model.HasPhysics());
		scene.Reset();
		EXPECT_EQ(scene.Step(1.0f / 60.0f), 1);
		model.Reset();
		EXPECT_FALSE(model.HasPhysics());
	}

	TEST_F(AnimationModelContractTest, IgnoresInvalidPhysicsElapsedTime) {
		Model model;
		RigidBodyDefinition rigidBody{};
		rigidBody.shapeSize = glm::vec3(1);
		ASSERT_TRUE(model.InitializePhysics(scene, {rigidBody}, {}));
		EXPECT_EQ(scene.Step(-1.0f), 0);
		EXPECT_EQ(scene.Step(std::numeric_limits<float>::quiet_NaN()), 0);
		EXPECT_TRUE(model.HasPhysics());
	}

	TEST_F(AnimationModelContractTest, RejectsInvalidPhysicsWithoutDiscardingTheCurrentWorld) {
		Model model;
		RigidBodyDefinition validRigidBody{};
		validRigidBody.shapeSize = glm::vec3(1);
		ASSERT_TRUE(model.InitializePhysics(scene, {validRigidBody}, {}));
		RigidBodyDefinition invalidRigidBody = validRigidBody;
		invalidRigidBody.group = 16;
		const auto result = model.InitializePhysics(scene, {invalidRigidBody}, {});
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
		const auto result = model.InitializePhysics(scene, {rigidBody}, {joint});
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
		AddNode(model);
		PreparePose(model);
		ModelUpdater::UpdateSkinning(model, 0);
		const glm::vec3 normal = model.geometryData.updateNormals.front();
		EXPECT_TRUE(std::isfinite(normal.x));
		EXPECT_TRUE(std::isfinite(normal.y));
		EXPECT_TRUE(std::isfinite(normal.z));
		EXPECT_EQ(normal, glm::vec3(0));
	}

	TEST_F(AnimationModelContractTest, KeepsDegenerateIkDirectionsFinite) {
		for (const bool oppositeDirections : {false, true}) {
			Node ikNode;
			Node targetNode;
			Node chainNode;
			if (oppositeDirections) {
				ikNode.global = glm::translate(glm::mat4(1), glm::vec3(-1, 0, 0));
				targetNode.global = glm::translate(glm::mat4(1), glm::vec3(1, 0, 0));
			}
			IkSolver solver;
			solver.ikNode = &ikNode;
			solver.ikTarget = &targetNode;
			solver.limitAngle = glm::pi<float>();
			auto& chain = solver.chains.emplace_back();
			chain.node = &chainNode;
			chain.saveIkRot = glm::quat(1, 0, 0, 0);
			solver.Solve();
			EXPECT_TRUE(IsFinite(chainNode.ikRotate));
		}
	}

	TEST_F(AnimationModelContractTest, UpdatesDeepChildTransformsWithoutTraversalStorage) {
		Node root;
		Node child;
		Node grandchild;
		root.AddChild(child);
		child.AddChild(grandchild);
		root.global = glm::translate(glm::mat4(1), glm::vec3(1, 0, 0));
		child.local = glm::translate(glm::mat4(1), glm::vec3(0, 2, 0));
		grandchild.local = glm::translate(glm::mat4(1), glm::vec3(0, 0, 3));
		root.UpdateChildTransform();
		EXPECT_EQ(glm::vec3(child.global[3]), glm::vec3(1, 2, 0));
		EXPECT_EQ(glm::vec3(grandchild.global[3]), glm::vec3(1, 2, 3));
	}

	TEST_F(AnimationModelContractTest, UpdatesEverySiblingTransform) {
		Node root;
		Node firstChild;
		Node secondChild;
		root.AddChild(firstChild);
		root.AddChild(secondChild);
		root.global = glm::translate(glm::mat4(1), glm::vec3(1, 0, 0));
		firstChild.local = glm::translate(glm::mat4(1), glm::vec3(0, 2, 0));
		secondChild.local = glm::translate(glm::mat4(1), glm::vec3(0, 0, 3));
		root.UpdateChildTransform();
		EXPECT_EQ(glm::vec3(firstChild.global[3]), glm::vec3(1, 2, 0));
		EXPECT_EQ(glm::vec3(secondChild.global[3]), glm::vec3(1, 0, 3));
	}
}
