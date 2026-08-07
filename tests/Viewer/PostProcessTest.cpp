#include "Viewer/PostProcess/PostProcess.h"

#include <gtest/gtest.h>
#include <limits>
#include <type_traits>
#include <utility>

namespace Chrivent {
	// GPU 리소스 없이 공통 후처리 실행 계약을 검증할 수 있도록 보호 인터페이스를 노출한다.
	class PostProcessTestAdapter final : public PostProcess {
	public:
		size_t GetProgramCount() const { return GetShaderPrograms().size(); }
		size_t GetPassCount() const { return GetPassRoutes().size(); }
		size_t GetResourceCount() const { return GetResourcePlans().size(); }
		float GetParameterValue(const size_t routeIndex, const size_t slot) const {
			return GetParameterData(GetPassRoutes()[routeIndex]).values[slot];
		}
		size_t GetResourceReadIndex(const size_t resourceIndex) const {
			return ResolveResourceReadIndex(resourceIndex);
		}
		size_t GetResourceWriteIndex(const size_t resourceIndex) const {
			return ResolveResourceWriteIndex(resourceIndex);
		}
		bool IsHistoryInitializationNeeded(const size_t resourceIndex) const {
			return NeedsHistoryInitialization(resourceIndex);
		}
		bool IsResourceInput(const size_t routeIndex, const size_t inputIndex) const {
			return GetPassRoutes()[routeIndex].inputs[inputIndex].kind == InputKind::Resource;
		}
		size_t GetInputResourceIndex(const size_t routeIndex, const size_t inputIndex) const {
			return GetPassRoutes()[routeIndex].inputs[inputIndex].resourceIndex;
		}
		size_t GetOutputResourceIndex(const size_t routeIndex) const {
			return GetPassRoutes()[routeIndex].outputResourceIndex;
		}

		// 테스트 효과 정의를 API 독립 실행 계획으로 변환한다.
		std::expected<void, PostProcessPlanError> Configure(
			const std::span<const EffectRuntimeDefinition* const> effects) {
			auto preparedEffectsResult = PreparedPostProcessEffects::Prepare(effects);
			if (!preparedEffectsResult)
				return std::unexpected(preparedEffectsResult.error());
			AdoptPreparedEffects(std::move(*preparedEffectsResult));
			return {};
		}

		// 지정한 패스의 출력 해상도를 계산한다.
		void ResolveExtent(const size_t routeIndex, int& width, int& height) const {
			ResolveOutputExtent(GetPassRoutes()[routeIndex], width, height);
		}

		// 테스트용 history 프레임 변경을 시작한다.
		void BeginTestHistoryFrame() {
			BeginHistoryFrame();
		}

		// 지정한 패스가 출력하는 history 리소스를 다음 색인으로 전환한다.
		void AdvanceTestHistory(const size_t routeIndex) {
			AdvanceHistory(GetPassRoutes()[routeIndex]);
		}

		// 지정한 history 리소스를 초기화 완료 상태로 기록한다.
		void MarkTestHistoryInitialized(const size_t resourceIndex) {
			MarkHistoryInitialized(resourceIndex);
		}

		// 테스트 어댑터에는 해제할 API별 GPU 리소스가 없다.
		void ResetResources() override {}
	};

	// 유효한 테스트용 셰이더 프로그램 선언을 생성한다.
	ShaderProgramDefinition MakeTestProgram() {
		return {
			.shaderPath = "test.hlsl",
			.vertexEntry = "VSMain",
			.pixelEntry = "PSMain"
		};
	}

	// 지정한 입력과 출력으로 테스트용 후처리 패스를 생성한다.
	EffectPassDefinition MakeTestPass(
		std::vector<EffectPassInputDefinition> inputs, const EffectPassOutputDefinition output) {
		return {
			.program = MakeTestProgram(),
			.inputs = std::move(inputs),
			.output = output
		};
	}

	// 장면 색상을 최종 출력으로 전달하는 단일 패스 효과를 생성한다.
	EffectRuntimeDefinition MakeSinglePassEffect() {
		EffectRuntimeDefinition effect;
		effect.passes.emplace_back(MakeTestPass(
			{ { .slot = 0, .kind = EffectPassInputKind::EffectInput } },
			{ .kind = EffectPassOutputKind::EffectOutput }));
		return effect;
	}

	TEST(PostProcessContract, PreparedEffectsRequireValidationFactory) {
		EXPECT_FALSE((std::is_constructible_v<PreparedPostProcessEffects,
			PreparedPostProcessEffects::ExecutionPlan>));
	}

	TEST(PostProcessContract, BuildsSinglePassInputsAndParameters) {
		PostProcessTestAdapter postProcess;
		EffectRuntimeDefinition effect = MakeSinglePassEffect();
		effect.parameters.emplace_back(EffectParameterValue{ .slot = 2, .value = 0.75f });
		effect.passes[0].inputs.emplace_back(
			EffectPassInputDefinition{ .slot = 1, .kind = EffectPassInputKind::SceneDepth });
		effect.passes[0].inputs.emplace_back(
			EffectPassInputDefinition{ .slot = 2, .kind = EffectPassInputKind::SceneVelocity });
		const std::vector<const EffectRuntimeDefinition*> effects{ &effect };
		const auto result = postProcess.Configure(effects);
		ASSERT_TRUE(result.has_value()) << result.error().Format();
		EXPECT_TRUE(postProcess.HasEffects());
		EXPECT_TRUE(postProcess.RequiresDepth());
		EXPECT_TRUE(postProcess.RequiresVelocity());
		EXPECT_EQ(postProcess.GetProgramCount(), 1);
		EXPECT_EQ(postProcess.GetPassCount(), 1);
		EXPECT_FLOAT_EQ(postProcess.GetParameterValue(0, 2), 0.75f);
		constexpr EffectParameterUpdate updates[]{
			{ .effectIndex = 0, .slot = 2, .value = 0.25f }
		};
		EXPECT_TRUE(postProcess.UpdateParameters(updates));
		EXPECT_FLOAT_EQ(postProcess.GetParameterValue(0, 2), 0.25f);
	}

	TEST(PostProcessContract, RejectsInvalidParameterDeclarationsAndUpdates) {
		PostProcessTestAdapter postProcess;
		EffectRuntimeDefinition effect = MakeSinglePassEffect();
		effect.parameters = {
			{ .slot = 1, .value = 0.25f },
			{ .slot = 1, .value = 0.5f }
		};
		const std::vector<const EffectRuntimeDefinition*> effects{ &effect };
		const auto declarationResult = postProcess.Configure(effects);
		ASSERT_FALSE(declarationResult.has_value());
		EXPECT_EQ(declarationResult.error().code, PostProcessPlanErrorCode::InvalidParameter);
		EXPECT_EQ(declarationResult.error().effectIndex, 0);
		EXPECT_EQ(declarationResult.error().passIndex, PostProcessPlanError::noPassIndex);
		effect.parameters = { { .slot = 1, .value = 0.25f } };
		ASSERT_TRUE(postProcess.Configure(effects).has_value());
		constexpr EffectParameterUpdate invalidEffect[]{
			{ .effectIndex = 1, .slot = 1, .value = 0.5f }
		};
		constexpr EffectParameterUpdate invalidSlot[]{
			{ .effectIndex = 0, .slot = PostProcessInputLayout::maxParameterCount, .value = 0.5f }
		};
		constexpr EffectParameterUpdate invalidValue[]{
			{ .effectIndex = 0, .slot = 1, .value = std::numeric_limits<float>::infinity() }
		};
		EXPECT_FALSE(postProcess.UpdateParameters(invalidEffect));
		EXPECT_FALSE(postProcess.UpdateParameters(invalidSlot));
		EXPECT_FALSE(postProcess.UpdateParameters(invalidValue));
		EXPECT_FLOAT_EQ(postProcess.GetParameterValue(0, 1), 0.25f);
	}

	TEST(PostProcessContract, RejectsDuplicateTextureSlots) {
		PostProcessTestAdapter postProcess;
		EffectRuntimeDefinition effect;
		effect.passes.emplace_back(MakeTestPass({
			{ .slot = 0, .kind = EffectPassInputKind::SceneColor },
			{ .slot = 0, .kind = EffectPassInputKind::SceneDepth }
		}, { .kind = EffectPassOutputKind::EffectOutput }));
		const std::vector<const EffectRuntimeDefinition*> effects{ &effect };
		EXPECT_FALSE(postProcess.Configure(effects).has_value());
	}

	TEST(PostProcessContract, RejectsReadBeforeTransientResourceWrite) {
		PostProcessTestAdapter postProcess;
		EffectRuntimeDefinition effect;
		effect.resources.emplace_back(EffectResourceDefinition{
			.lifetime = EffectResourceLifetime::Transient,
			.format = EffectTextureFormat::Rgba16Float,
			.resolution = EffectPassResolution::Half
		});
		effect.passes.emplace_back(MakeTestPass(
			{ { .slot = 0, .kind = EffectPassInputKind::Resource, .resourceIndex = 0 } },
			{ .kind = EffectPassOutputKind::EffectOutput }));
		const std::vector<const EffectRuntimeDefinition*> effects{ &effect };
		EXPECT_FALSE(postProcess.Configure(effects).has_value());
	}

	TEST(PostProcessContract, ResolvesTransientPassOrderAndExtent) {
		PostProcessTestAdapter postProcess;
		EffectRuntimeDefinition effect;
		effect.resources.emplace_back(EffectResourceDefinition{
			.lifetime = EffectResourceLifetime::Transient,
			.format = EffectTextureFormat::Rgba16Float,
			.resolution = EffectPassResolution::Half
		});
		effect.passes.emplace_back(MakeTestPass(
			{ { .slot = 0, .kind = EffectPassInputKind::SceneColor } },
			{ .kind = EffectPassOutputKind::Resource, .resourceIndex = 0 }));
		effect.passes.emplace_back(MakeTestPass(
			{ { .slot = 0, .kind = EffectPassInputKind::Resource, .resourceIndex = 0 } },
			{ .kind = EffectPassOutputKind::EffectOutput }));
		const std::vector<const EffectRuntimeDefinition*> effects{ &effect };
		const auto result = postProcess.Configure(effects);
		ASSERT_TRUE(result.has_value()) << result.error().Format();
		EXPECT_EQ(postProcess.GetPassCount(), 2);
		EXPECT_EQ(postProcess.GetResourceCount(), 1);
		int width = 1921;
		int height = 1081;
		postProcess.ResolveExtent(0, width, height);
		EXPECT_EQ(width, 961);
		EXPECT_EQ(height, 541);
		width = 1921;
		height = 1081;
		postProcess.ResolveExtent(1, width, height);
		EXPECT_EQ(width, 1921);
		EXPECT_EQ(height, 1081);
	}

	TEST(PostProcessContract, CommitsDiscardsAndResetsHistoryState) {
		PostProcessTestAdapter postProcess;
		EffectRuntimeDefinition effect;
		effect.resources.emplace_back(EffectResourceDefinition{
			.lifetime = EffectResourceLifetime::History,
			.format = EffectTextureFormat::Rgba16Float,
			.resolution = EffectPassResolution::Full
		});
		effect.passes.emplace_back(MakeTestPass(
			{ { .slot = 0, .kind = EffectPassInputKind::Resource, .resourceIndex = 0 } },
			{ .kind = EffectPassOutputKind::Resource, .resourceIndex = 0 }));
		effect.passes.emplace_back(MakeTestPass(
			{ { .slot = 0, .kind = EffectPassInputKind::Resource, .resourceIndex = 0 } },
			{ .kind = EffectPassOutputKind::EffectOutput }));
		const std::vector<const EffectRuntimeDefinition*> effects{ &effect };
		ASSERT_TRUE(postProcess.Configure(effects).has_value());
		EXPECT_TRUE(postProcess.IsHistoryInitializationNeeded(0));
		postProcess.MarkTestHistoryInitialized(0);
		EXPECT_FALSE(postProcess.IsHistoryInitializationNeeded(0));
		EXPECT_EQ(postProcess.GetResourceReadIndex(0), 0);
		EXPECT_EQ(postProcess.GetResourceWriteIndex(0), 1);
		postProcess.BeginTestHistoryFrame();
		postProcess.AdvanceTestHistory(0);
		EXPECT_EQ(postProcess.GetResourceReadIndex(0), 1);
		postProcess.DiscardHistoryFrame();
		EXPECT_EQ(postProcess.GetResourceReadIndex(0), 0);
		postProcess.BeginTestHistoryFrame();
		postProcess.AdvanceTestHistory(0);
		postProcess.CommitHistoryFrame();
		EXPECT_EQ(postProcess.GetResourceReadIndex(0), 1);
		postProcess.ResetHistory();
		EXPECT_TRUE(postProcess.IsHistoryInitializationNeeded(0));
		EXPECT_EQ(postProcess.GetResourceReadIndex(0), 0);
	}

	TEST(PostProcessContract, ChainsMultipleEffectsThroughIntermediateResource) {
		PostProcessTestAdapter postProcess;
		EffectRuntimeDefinition firstEffect = MakeSinglePassEffect();
		EffectRuntimeDefinition secondEffect = MakeSinglePassEffect();
		const std::vector<const EffectRuntimeDefinition*> effects{ &firstEffect, &secondEffect };
		const auto result = postProcess.Configure(effects);
		ASSERT_TRUE(result.has_value()) << result.error().Format();
		ASSERT_EQ(postProcess.GetPassCount(), 2);
		ASSERT_EQ(postProcess.GetResourceCount(), 1);
		EXPECT_TRUE(postProcess.IsResourceInput(1, 0));
		EXPECT_EQ(postProcess.GetInputResourceIndex(1, 0),
			postProcess.GetOutputResourceIndex(0));
	}
}
