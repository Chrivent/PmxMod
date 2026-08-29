#include "Viewer/Viewer/Viewer.h"

#include "Core/Model/Model.h"
#include "Viewer/Drawer/Drawer.h"
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
			const std::span<const EffectRuntimeDefinition* const> effects) {
			auto preparedEffectsResult = PreparedPostProcessEffects::Prepare(effects);
			if (!preparedEffectsResult)
				return std::unexpected(preparedEffectsResult.error());
			AdoptPreparedEffects(std::move(*preparedEffectsResult));
			return {};
		}

		// 이미 검증한 테스트 실행 계획을 현재 상태로 적용한다.
		void ConfigurePreparedEffects(PreparedPostProcessEffects preparedEffects) {
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
		bool failLoad = false;
		bool failResize = false;
		bool failWait = false;
		bool skipBegin = false;
		bool skipEnd = false;
		bool resizedWithDepthEffect = false;
		size_t beginCallCount = 0;
		size_t loadEffectCallCount = 0;
		size_t resizeCallCount = 0;
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
			PreparedPostProcessEffects preparedEffects) override {
			loadEffectCallCount++;
			if (failLoad) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::Unknown,
					GraphicsErrorCode::EffectConfigurationFailed,
					"테스트 후처리 효과 구성", "의도한 테스트 실패"));
			}
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
			resizeCallCount++;
			resizedWithDepthEffect = postProcess.HasEffects() && postProcess.RequiresDepth();
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
			return skipEnd ? FrameEndState::Skipped : FrameEndState::Presented;
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
		size_t GetResizeCallCount() const { return resizeCallCount; }
		size_t GetWaitCallCount() const { return waitCallCount; }
		bool WasResizedWithDepthEffect() const { return resizedWithDepthEffect; }
		// 현재 프레임에서 요구하는 후처리 장면 입력 패스를 정상적으로 완료한다.
		void CompleteSceneInput() {
			const auto beginResult = BeginPostProcessSceneInputPass();
			ASSERT_TRUE(beginResult.has_value());
			ASSERT_EQ(*beginResult, PostProcessSceneInputState::Ready);
			ASSERT_TRUE(EndPostProcessSceneInputPass().has_value());
		}
		void SetBeginFailure(const bool fail) { failBegin = fail; }
		void SetBeginSceneInputFailure(const bool fail) { failBeginSceneInput = fail; }
		void SetEndFailure(const bool fail) { failEnd = fail; }
		void SetEndSceneInputFailure(const bool fail) { failEndSceneInput = fail; }
		void SetLoadFailure(const bool fail) { failLoad = fail; }
		void SetResizeFailure(const bool fail) { failResize = fail; }
		void SetWaitFailure(const bool fail) { failWait = fail; }
		void SetSkipBegin(const bool skip) { skipBegin = skip; }
		void SetSkipEnd(const bool skip) { skipEnd = skip; }
	};

	// GPU 없이 Instance의 초기화와 공개 작업 계약을 검증하는 드로어 구현이다.
	class InstanceTestDrawer final : public Drawer {
	protected:
		// 테스트 모델 패스는 GPU 명령 없이 성공한다.
		GraphicsError::Result<void> DrawModel() override { return {}; }
		// 테스트 엣지 패스는 GPU 명령 없이 성공한다.
		GraphicsError::Result<void> DrawEdge() override { return {}; }
		// 테스트 지면 그림자 패스는 GPU 명령 없이 성공한다.
		GraphicsError::Result<void> DrawGroundShadow() override { return {}; }
		// 테스트 장면 입력 패스는 GPU 명령 없이 성공한다.
		GraphicsError::Result<void> DrawSceneInputs() override { return {}; }

	public:
		InstanceTestDrawer() : Drawer(GraphicsApi::Unknown) {}
	};

	// API 리소스 생성 성공과 실패를 주입해 Instance의 롤백 계약을 검증한다.
	class InstanceTestAdapter final : public Instance {
		bool setupFailure = false;
		size_t resetCallCount = 0;
		size_t uploadCallCount = 0;

	protected:
		// 테스트 리소스 초기화 횟수를 기록한다.
		void ResetRendererResources() override { resetCallCount++; }
		// 설정한 테스트 상태에 따라 리소스 초기화 결과를 반환한다.
		GraphicsError::Result<void> SetupRenderer() override {
			if (!setupFailure)
				return {};
			return std::unexpected(CreateGraphicsError(GraphicsErrorCode::ResourceCreationFailed,
				"테스트 모델 리소스 생성", "의도한 테스트 실패"));
		}
		// 테스트 버텍스 업로드 횟수를 기록한다.
		GraphicsError::Result<void> UploadCore() override {
			uploadCallCount++;
			return {};
		}

	public:
		InstanceTestAdapter() : Instance(GraphicsApi::Unknown) {
			drawer = std::make_unique<InstanceTestDrawer>();
		}

		bool HasBoundModel() const { return model != nullptr; }
		size_t GetResetCallCount() const { return resetCallCount; }
		size_t GetUploadCallCount() const { return uploadCallCount; }
		void SetSetupFailure(const bool fail) { setupFailure = fail; }
	};

	// Instance 공통 검증을 통과하는 최소 모델을 생성한다.
	std::shared_ptr<Model> CreateInstanceTestModel() {
		auto model = std::make_shared<Model>();
		model->geometryData.positions.emplace_back(0.0f);
		model->geometryData.indices.emplace_back(0);
		model->geometryData.indexCount = 1;
		model->geometryData.indexElementSize = 1;
		model->materialData.materials.emplace_back();
		model->materialData.subMeshes.emplace_back(SubMesh{
			.beginIndex = 0,
			.indexCount = 1,
			.materialId = 0
		});
		return model;
	}

	// GLFW 객체를 사용하지 않는 공통 상태 테스트에 전달할 비어 있지 않은 핸들을 반환한다.
	GLFWwindow* ResolveTestWindow() {
		return reinterpret_cast<GLFWwindow*>(uintptr_t{1});
	}

	TEST(InstanceContract, RejectsPublicWorkBeforeInitialization) {
		InstanceTestAdapter instance;
		EXPECT_FALSE(instance.Upload().has_value());
		EXPECT_FALSE(instance.DrawModelPass().has_value());
		EXPECT_FALSE(instance.DrawEdgePass().has_value());
		EXPECT_FALSE(instance.DrawGroundShadowPass().has_value());
		EXPECT_FALSE(instance.DrawPostProcessSceneInputs().has_value());
		instance.BeginDraw({});
		instance.PrepareUpdate({});
		EXPECT_EQ(instance.CalculateSkinningTaskCount(), 0);
		instance.UpdateSkinning(0);
		EXPECT_EQ(instance.GetUploadCallCount(), 0);
	}

	TEST(InstanceContract, RollsBackFailedInitializationAndCanRetry) {
		InstanceTestAdapter instance;
		const std::shared_ptr<Model> model = CreateInstanceTestModel();
		instance.SetSetupFailure(true);
		const auto failureResult = instance.Initialize(model, 1.0f);
		ASSERT_FALSE(failureResult.has_value());
		EXPECT_FALSE(instance.HasBoundModel());
		EXPECT_FALSE(instance.Upload().has_value());
		EXPECT_EQ(instance.GetResetCallCount(), 2);
		instance.SetSetupFailure(false);
		ASSERT_TRUE(instance.Initialize(model, 1.0f).has_value());
		EXPECT_TRUE(instance.HasBoundModel());
		EXPECT_TRUE(instance.Upload().has_value());
		EXPECT_EQ(instance.GetUploadCallCount(), 1);
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
		viewer.CompleteSceneInput();
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

	TEST(ViewerContract, ActiveEffectSurvivesRepeatedResizeAndResetsTemporalHistory) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		viewer.GetSceneRenderState().viewMatrix[3].x = 1.0f;
		ASSERT_TRUE(viewer.BeginFrame().has_value());
		viewer.CompleteSceneInput();
		ASSERT_TRUE(viewer.EndFrame().has_value());
		ASSERT_FALSE(viewer.IsPostProcessHistoryResetPending());
		ASSERT_TRUE(viewer.Resize(1920, 1080).has_value());
		EXPECT_EQ(viewer.GetResizeCallCount(), 1);
		EXPECT_TRUE(viewer.WasResizedWithDepthEffect());
		EXPECT_TRUE(viewer.IsPostProcessHistoryResetPending());
		ASSERT_TRUE(viewer.BeginFrame().has_value());
		viewer.CompleteSceneInput();
		ASSERT_TRUE(viewer.EndFrame().has_value());
		EXPECT_FALSE(viewer.IsPostProcessHistoryResetPending());
		ASSERT_TRUE(viewer.Resize(1280, 720).has_value());
		EXPECT_EQ(viewer.GetResizeCallCount(), 2);
		EXPECT_TRUE(viewer.WasResizedWithDepthEffect());
		ASSERT_TRUE(viewer.BeginFrame().has_value());
		viewer.CompleteSceneInput();
		EXPECT_TRUE(viewer.EndFrame().has_value());
	}

	TEST(ViewerContract, ResizeDuringFrameIsRejectedWithoutInvalidatingRenderer) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		ASSERT_TRUE(viewer.BeginFrame().has_value());
		const auto resizeResult = viewer.Resize(1920, 1080);
		ASSERT_FALSE(resizeResult.has_value());
		EXPECT_EQ(resizeResult.error().code, GraphicsErrorCode::InvalidState);
		EXPECT_EQ(viewer.GetResizeCallCount(), 0);
		viewer.CompleteSceneInput();
		ASSERT_TRUE(viewer.EndFrame().has_value());
		EXPECT_TRUE(viewer.Resize(1920, 1080).has_value());
		EXPECT_EQ(viewer.GetResizeCallCount(), 1);
	}

	TEST(ViewerContract, EffectValidationFailureKeepsRendererUsable) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		constexpr const EffectRuntimeDefinition* invalidEffects[]{ nullptr };
		EXPECT_FALSE(viewer.LoadPostProcessEffects(invalidEffects).has_value());
		EXPECT_EQ(viewer.GetWaitCallCount(), 0);
		EXPECT_EQ(viewer.GetLoadEffectCallCount(), 0);
		const auto beginResult = viewer.BeginFrame();
		ASSERT_TRUE(beginResult.has_value());
		EXPECT_EQ(*beginResult, FrameBeginState::Ready);
		viewer.CompleteSceneInput();
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

	TEST(ViewerContract, EffectResourceFailureKeepsPreviousChainAndRendererUsable) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		viewer.SetLoadFailure(true);
		EXPECT_FALSE(viewer.LoadPostProcessEffects({}).has_value());
		EXPECT_EQ(viewer.GetWaitCallCount(), 1);
		EXPECT_EQ(viewer.GetLoadEffectCallCount(), 1);
		const auto beginResult = viewer.BeginFrame();
		ASSERT_TRUE(beginResult.has_value());
		ASSERT_EQ(*beginResult, FrameBeginState::Ready);
		const auto sceneInputResult = viewer.BeginPostProcessSceneInputPass();
		ASSERT_TRUE(sceneInputResult.has_value());
		ASSERT_EQ(*sceneInputResult, PostProcessSceneInputState::Ready);
		ASSERT_TRUE(viewer.EndPostProcessSceneInputPass().has_value());
		const auto endResult = viewer.EndFrame();
		ASSERT_TRUE(endResult.has_value());
		EXPECT_EQ(*endResult, FrameEndState::Presented);
	}

	TEST(ViewerContract, SceneInputIsNotRequiredWithoutEffects) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		ASSERT_TRUE(viewer.LoadPostProcessEffects({}).has_value());
		const auto beginResult = viewer.BeginFrame();
		ASSERT_TRUE(beginResult.has_value());
		ASSERT_EQ(*beginResult, FrameBeginState::Ready);
		const auto sceneInputResult = viewer.BeginPostProcessSceneInputPass();
		ASSERT_TRUE(sceneInputResult.has_value());
		EXPECT_EQ(*sceneInputResult, PostProcessSceneInputState::NotRequired);
		ASSERT_TRUE(viewer.EndFrame().has_value());
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
		viewer.CompleteSceneInput();
		ASSERT_TRUE(viewer.EndFrame().has_value());
	}

	TEST(ViewerContract, RequiredSceneInputMustCompleteBeforeFrameEnd) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		ASSERT_TRUE(viewer.BeginFrame().has_value());
		const auto incompleteResult = viewer.EndFrame();
		ASSERT_FALSE(incompleteResult.has_value());
		EXPECT_EQ(incompleteResult.error().code, GraphicsErrorCode::InvalidState);
		viewer.CompleteSceneInput();
		EXPECT_TRUE(viewer.EndFrame().has_value());
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
		viewer.CompleteSceneInput();
		const auto endResult = viewer.EndFrame();
		ASSERT_TRUE(endResult.has_value());
		EXPECT_EQ(*endResult, FrameEndState::Presented);
	}

	TEST(ViewerContract, SkippedPresentationDoesNotCommitTemporalHistory) {
		ViewerTestAdapter viewer;
		ASSERT_TRUE(viewer.Setup(ResolveTestWindow(), 1280, 720, {}).has_value());
		viewer.GetSceneRenderState().viewMatrix[3].x = 1.0f;
		ASSERT_TRUE(viewer.BeginFrame().has_value());
		viewer.CompleteSceneInput();
		const auto presentedResult = viewer.EndFrame();
		ASSERT_TRUE(presentedResult.has_value());
		ASSERT_EQ(*presentedResult, FrameEndState::Presented);
		ASSERT_FLOAT_EQ(viewer.GetPreviousViewMatrix()[3].x, 1.0f);
		ASSERT_FALSE(viewer.IsPostProcessHistoryResetPending());
		viewer.GetSceneRenderState().viewMatrix[3].x = 2.0f;
		viewer.SetSkipEnd(true);
		ASSERT_TRUE(viewer.BeginFrame().has_value());
		viewer.CompleteSceneInput();
		const auto skippedResult = viewer.EndFrame();
		ASSERT_TRUE(skippedResult.has_value());
		EXPECT_EQ(*skippedResult, FrameEndState::Skipped);
		EXPECT_FLOAT_EQ(viewer.GetPreviousViewMatrix()[3].x, 1.0f);
		EXPECT_FALSE(viewer.IsPostProcessHistoryResetPending());
		viewer.SetSkipEnd(false);
		ASSERT_TRUE(viewer.BeginFrame().has_value());
		viewer.CompleteSceneInput();
		const auto resumedResult = viewer.EndFrame();
		ASSERT_TRUE(resumedResult.has_value());
		EXPECT_EQ(*resumedResult, FrameEndState::Presented);
		EXPECT_FLOAT_EQ(viewer.GetPreviousViewMatrix()[3].x, 2.0f);
	}
}
