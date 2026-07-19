#include "Viewer/Viewer/Viewer.h"

#include "Viewer/Instance/Instance.h"
#include "Viewer/PostProcess/PostProcess.h"

#include <gtest/gtest.h>
#include <utility>

namespace Chrivent {
	// GPU 리소스 없이 Viewer 상태 전이를 검증하는 후처리 구현이다.
	class ViewerTestPostProcess final : public PostProcess {
	public:
		float GetParameterValue(const size_t slot) const {
			return GetParameterData(GetPassRoutes().front()).values[slot];
		}

		// depth 입력이 필요한 최소 실행 계획을 구성한다.
		std::expected<void, PostProcessPlanError> ConfigureDepthEffect(
			const std::vector<const EffectRuntimeDefinition*>& effects) {
			auto preparedEffectsResult = PrepareEffects(effects);
			if (!preparedEffectsResult)
				return std::unexpected(preparedEffectsResult.error());
			AdoptPreparedEffects(std::move(*preparedEffectsResult));
			return {};
		}

		// 이미 검증한 테스트 실행 계획을 현재 상태로 적용한다.
		void ConfigurePreparedEffects(PreparedEffects preparedEffects) {
			AdoptPreparedEffects(std::move(preparedEffects));
		}

		// 테스트 구현에는 해제할 API 리소스가 없다.
		void ResetResources() override {}
	};

	// API별 성공과 실패를 주입해 Viewer 공통 상태 계약을 검증한다.
	class ViewerTestAdapter final : public Viewer {
		ViewerTestPostProcess postProcess;
		EffectRuntimeDefinition depthEffect;
		bool failBegin = false;
		bool failEnd = false;
		bool failBeginSceneInput = false;
		bool failEndSceneInput = false;
		bool failResize = false;
		bool failWait = false;
		bool skipBegin = false;
		size_t beginCallCount = 0;
		size_t loadEffectCallCount = 0;
		size_t waitCallCount = 0;

		// 테스트에서 요청한 실패를 구조화된 그래픽 오류로 생성한다.
		static GraphicsError::Result<void> ResolveTestResult(const bool fail, const std::string& operation) {
			if (!fail)
				return {};
			return std::unexpected(GraphicsError::Create(GraphicsApi::Unknown,
				GraphicsErrorCode::CommandRecordingFailed, operation, "의도한 테스트 실패"));
		}

	protected:
		// 테스트 효과 구성은 API 리소스를 만들지 않는다.
		GraphicsError::Result<void> LoadPostProcessEffectsCore(
			PostProcess::PreparedEffects preparedEffects) override {
			loadEffectCallCount++;
			postProcess.ConfigurePreparedEffects(std::move(preparedEffects));
			return {};
		}

		// 테스트 장면 입력 시작 결과를 반환한다.
		GraphicsError::Result<void> BeginPostProcessSceneInputPassCore() override {
			return ResolveTestResult(failBeginSceneInput, "테스트 장면 입력 시작");
		}

		// 테스트 장면 입력 종료 결과를 반환한다.
		GraphicsError::Result<void> EndPostProcessSceneInputPassCore() override {
			return ResolveTestResult(failEndSceneInput, "테스트 장면 입력 종료");
		}

		// 공통 Viewer와 테스트 후처리를 연결한다.
		GraphicsError::Result<void> SetupCore(const SceneShaderRuntimeContract&) override {
			BindPostProcess(postProcess);
			depthEffect.passes.emplace_back(EffectPassDefinition{
				.program = {
					.shaderPath = "test.hlsl",
					.vertexEntry = "VSMain",
					.pixelEntry = "PSMain"
				},
				.inputs = {
					{ .slot = 0, .kind = EffectPassInputKind::SceneColor },
					{ .slot = 1, .kind = EffectPassInputKind::SceneDepth }
				},
				.output = { .kind = EffectPassOutputKind::EffectOutput }
			});
			const std::vector<const EffectRuntimeDefinition*> effects{ &depthEffect };
			const auto result = postProcess.ConfigureDepthEffect(effects);
			if (result)
				return {};
			return std::unexpected(GraphicsError::Create(GraphicsApi::Unknown,
				GraphicsErrorCode::ContractViolation, "테스트 후처리 구성", result.error().Format()));
		}

		// 테스트에서는 크기 의존 리소스를 만들지 않는다.
		GraphicsError::Result<void> ResizeCore() override {
			return ResolveTestResult(failResize, "테스트 크기 변경");
		}

		// 주입한 테스트 상태에 따라 프레임 시작 결과를 반환한다.
		GraphicsError::Result<FrameBeginState> BeginFrameCore() override {
			beginCallCount++;
			if (failBegin)
				return std::unexpected(GraphicsError::Create(GraphicsApi::Unknown,
					GraphicsErrorCode::CommandRecordingFailed, "테스트 프레임 시작", "의도한 테스트 실패"));
			return skipBegin ? FrameBeginState::Skipped : FrameBeginState::Ready;
		}

		// 주입한 테스트 상태에 따라 프레임 종료 결과를 반환한다.
		GraphicsError::Result<FrameEndState> EndFrameCore() override {
			if (failEnd)
				return std::unexpected(GraphicsError::Create(GraphicsApi::Unknown,
					GraphicsErrorCode::PresentationFailed, "테스트 프레임 종료", "의도한 테스트 실패"));
			return FrameEndState::Presented;
		}

		// 주입한 테스트 상태에 따라 GPU 대기 결과를 반환한다.
		GraphicsError::Result<void> WaitIdleCore() override {
			waitCallCount++;
			return ResolveTestResult(failWait, "테스트 GPU 대기");
		}

		// 테스트에서는 모델 인스턴스를 만들지 않는다.
		std::unique_ptr<Instance> CreateInstanceCore() override { return {}; }

	public:
		ViewerTestAdapter() : Viewer(GraphicsApi::Unknown, false) {}

		size_t GetBeginCallCount() const { return beginCallCount; }
		size_t GetLoadEffectCallCount() const { return loadEffectCallCount; }
		float GetParameterValue(const size_t slot) const { return postProcess.GetParameterValue(slot); }
		size_t GetWaitCallCount() const { return waitCallCount; }
		void SetBeginFailure(const bool fail) { failBegin = fail; }
		void SetBeginSceneInputFailure(const bool fail) { failBeginSceneInput = fail; }
		void SetEndFailure(const bool fail) { failEnd = fail; }
		void SetEndSceneInputFailure(const bool fail) { failEndSceneInput = fail; }
		void SetResizeFailure(const bool fail) { failResize = fail; }
		void SetWaitFailure(const bool fail) { failWait = fail; }
		void SetSkipBegin(const bool skip) { skipBegin = skip; }
	};

	// GLFW 객체를 사용하지 않는 공통 상태 테스트에 전달할 비어 있지 않은 핸들을 반환한다.
	GLFWwindow* ResolveTestWindow() {
		return reinterpret_cast<GLFWwindow*>(uintptr_t{1});
	}

	TEST(ViewerContract, BeginFailureInvalidatesRenderer) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		viewer.SetBeginFailure(true);
		EXPECT_FALSE(viewer.BeginFrame().has_value());
		viewer.SetBeginFailure(false);
		EXPECT_FALSE(viewer.BeginFrame().has_value());
		EXPECT_EQ(viewer.GetBeginCallCount(), 1);
	}

	TEST(ViewerContract, EndFailureInvalidatesRenderer) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		const auto beginResult = viewer.BeginFrame();
		ASSERT_TRUE(beginResult.has_value());
		ASSERT_EQ(*beginResult, FrameBeginState::Ready);
		viewer.SetEndFailure(true);
		EXPECT_FALSE(viewer.EndFrame().has_value());
		EXPECT_FALSE(viewer.BeginFrame().has_value());
	}

	TEST(ViewerContract, SceneInputFailureInvalidatesRenderer) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		const auto beginResult = viewer.BeginFrame();
		ASSERT_TRUE(beginResult.has_value());
		ASSERT_EQ(*beginResult, FrameBeginState::Ready);
		const auto sceneInputResult = viewer.BeginPostProcessSceneInputPass();
		ASSERT_TRUE(sceneInputResult.has_value());
		ASSERT_EQ(*sceneInputResult, PostProcessSceneInputState::Ready);
		viewer.SetEndSceneInputFailure(true);
		EXPECT_FALSE(viewer.EndPostProcessSceneInputPass().has_value());
		EXPECT_FALSE(viewer.EndFrame().has_value());
	}

	TEST(ViewerContract, SceneInputBeginFailureInvalidatesRenderer) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		const auto beginResult = viewer.BeginFrame();
		ASSERT_TRUE(beginResult.has_value());
		ASSERT_EQ(*beginResult, FrameBeginState::Ready);
		viewer.SetBeginSceneInputFailure(true);
		EXPECT_FALSE(viewer.BeginPostProcessSceneInputPass().has_value());
		EXPECT_FALSE(viewer.EndFrame().has_value());
	}

	TEST(ViewerContract, WaitFailureInvalidatesRenderer) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		viewer.SetWaitFailure(true);
		EXPECT_FALSE(viewer.WaitIdle().has_value());
		viewer.SetWaitFailure(false);
		EXPECT_FALSE(viewer.BeginFrame().has_value());
	}

	TEST(ViewerContract, ResizeFailureInvalidatesRenderer) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		viewer.SetResizeFailure(true);
		EXPECT_FALSE(viewer.Resize(1920, 1080).has_value());
		viewer.SetResizeFailure(false);
		EXPECT_FALSE(viewer.BeginFrame().has_value());
	}

	TEST(ViewerContract, EffectValidationFailureKeepsRendererUsable) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		EXPECT_FALSE(viewer.LoadPostProcessEffects({ nullptr }).has_value());
		EXPECT_EQ(viewer.GetWaitCallCount(), 0);
		EXPECT_EQ(viewer.GetLoadEffectCallCount(), 0);
		const auto beginResult = viewer.BeginFrame();
		ASSERT_TRUE(beginResult.has_value());
		EXPECT_EQ(*beginResult, FrameBeginState::Ready);
		const auto endResult = viewer.EndFrame();
		ASSERT_TRUE(endResult.has_value());
		EXPECT_EQ(*endResult, FrameEndState::Presented);
	}

	TEST(ViewerContract, ValidEffectReloadWaitsBeforeCreatingApiResources) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());

		ASSERT_TRUE(viewer.LoadPostProcessEffects({}).has_value());

		EXPECT_EQ(viewer.GetWaitCallCount(), 1);
		EXPECT_EQ(viewer.GetLoadEffectCallCount(), 1);
	}

	TEST(ViewerContract, ParameterUpdatesAreCoalescedAndAppliedAtFrameBoundary) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		constexpr EffectParameterUpdate updates[]{
			{ .effectIndex = 0, .slot = 2, .value = 0.25f },
			{ .effectIndex = 0, .slot = 2, .value = 0.75f }
		};

		ASSERT_TRUE(viewer.UpdatePostProcessParameters(updates).has_value());
		EXPECT_FLOAT_EQ(viewer.GetParameterValue(2), 0.0f);
		const auto beginResult = viewer.BeginFrame();

		ASSERT_TRUE(beginResult.has_value());
		ASSERT_EQ(*beginResult, FrameBeginState::Ready);
		EXPECT_FLOAT_EQ(viewer.GetParameterValue(2), 0.75f);
		ASSERT_TRUE(viewer.EndFrame().has_value());
	}

	TEST(ViewerContract, SkippedFrameKeepsRendererUsable) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		viewer.SetSkipBegin(true);
		const auto skippedResult = viewer.BeginFrame();
		ASSERT_TRUE(skippedResult.has_value());
		ASSERT_EQ(*skippedResult, FrameBeginState::Skipped);
		viewer.SetSkipBegin(false);
		const auto beginResult = viewer.BeginFrame();
		ASSERT_TRUE(beginResult.has_value());
		ASSERT_EQ(*beginResult, FrameBeginState::Ready);
		const auto endResult = viewer.EndFrame();
		ASSERT_TRUE(endResult.has_value());
		EXPECT_EQ(*endResult, FrameEndState::Presented);
	}
}
