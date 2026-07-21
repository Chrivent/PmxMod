#pragma once

#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/PostProcess/PostProcessRuntimeContract.h"

#include <expected>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace Chrivent {
	// 후처리 실행 계획을 만들지 못한 원인을 안정된 범주로 구분한다.
	enum class PostProcessPlanErrorCode {
		InvalidEffect,
		InvalidParameter,
		InvalidResource,
		InvalidProgram,
		InvalidInput,
		InvalidOutput
	};

	// 잘못된 효과와 패스 위치 및 사용자 진단 메시지를 함께 보관한다.
	struct PostProcessPlanError {
		static constexpr size_t noPassIndex = std::numeric_limits<size_t>::max();

		PostProcessPlanErrorCode code = PostProcessPlanErrorCode::InvalidEffect;
		size_t effectIndex = 0;
		size_t passIndex = noPassIndex;
		std::string message;

		// 효과와 선택적 패스 위치를 포함한 사용자 진단 문자열을 생성한다.
		std::string Format() const;
	};

	// API 독립적인 후처리 패스 경로와 실행에 필요한 데이터를 함께 보관한다.
	struct PostProcessExecutionPlan {
		enum class InputKind {
			SceneColor,
			SceneDepth,
			SceneVelocity,
			Resource
		};

		enum class OutputKind {
			Present,
			Resource
		};

		// 효과 리소스 하나의 수명과 형식 및 실제 출력 크기를 나타낸다.
		struct Resource {
			EffectResourceLifetime lifetime = EffectResourceLifetime::Transient;
			EffectTextureFormat format = EffectTextureFormat::Rgba16Float;
			EffectPassResolution resolution = EffectPassResolution::Full;
			uint32_t width = 0;
			uint32_t height = 0;
		};

		// 패스의 texture 슬롯이 참조할 장면 또는 효과 리소스를 나타낸다.
		struct PassInputRoute {
			uint32_t slot = 0;
			InputKind kind = InputKind::SceneColor;
			size_t resourceIndex = 0;
		};

		// 한 효과가 b1 상수 버퍼로 전달할 스칼라 파라미터 값을 보관한다.
		struct ParameterData {
			float values[PostProcessInputLayout::maxParameterCount]{};
		};

		// 후처리 패스 하나의 입력 경로와 출력 대상을 나타낸다.
		struct PassRoute {
			std::vector<PassInputRoute> inputs;
			OutputKind outputKind = OutputKind::Present;
			size_t outputResourceIndex = 0;
			size_t effectIndex = 0;
		};

		std::vector<ShaderProgramDefinition> shaderPrograms;
		std::vector<PassRoute> passRoutes;
		std::vector<ParameterData> effectParameters;
		std::vector<Resource> resourcePlans;
		bool depthRequired = false;
		bool velocityRequired = false;
	};

	// 검증된 후처리 실행 계획을 API 후보 객체까지 이동시킨다.
	class PreparedPostProcessEffects {
		PostProcessExecutionPlan executionPlan;

	public:
		explicit PreparedPostProcessEffects(PostProcessExecutionPlan sourceExecutionPlan);

		PreparedPostProcessEffects(const PreparedPostProcessEffects&) = delete;
		PreparedPostProcessEffects& operator=(const PreparedPostProcessEffects&) = delete;
		PreparedPostProcessEffects(PreparedPostProcessEffects&&) = default;
		PreparedPostProcessEffects& operator=(PreparedPostProcessEffects&&) = default;

		// 보관한 실행 계획의 소유권을 호출자에게 반환한다.
		PostProcessExecutionPlan TakeExecutionPlan() &&;
	};

	// 패키지 효과를 API 독립적인 후처리 실행 계획으로 변환하고 상태를 관리한다.
	class PostProcess {
	protected:
		using InputKind = PostProcessExecutionPlan::InputKind;
		using OutputKind = PostProcessExecutionPlan::OutputKind;
		using ResourcePlan = PostProcessExecutionPlan::Resource;
		using PassInputRoute = PostProcessExecutionPlan::PassInputRoute;
		using ParameterData = PostProcessExecutionPlan::ParameterData;
		using PassRoute = PostProcessExecutionPlan::PassRoute;

	private:
		// history 리소스의 현재 읽기 인덱스와 초기화 여부를 기록한다.
		struct ResourceHistoryState {
			size_t readIndex = 0;
			bool initialized = false;
		};

		std::vector<ShaderProgramDefinition> shaderPrograms;
		std::vector<PassRoute> passRoutes;
		std::vector<ParameterData> effectParameters;
		std::vector<ResourcePlan> resourcePlans;
		std::vector<ResourceHistoryState> resourceHistoryStates;
		std::vector<ResourceHistoryState> pendingResourceHistoryStates;
		bool depthRequired = false;
		bool velocityRequired = false;
		bool historyFramePending = false;

		// history ping-pong 인덱스를 다음 write target으로 전환한다.
		static size_t ResolveNextHistoryIndex(size_t currentIndex);
		// 현재 패스 기록에서 사용할 committed 또는 pending history 상태를 반환한다.
		const std::vector<ResourceHistoryState>& ResolveHistoryStates() const;
		// 현재 패스 기록에서 수정할 committed 또는 pending history 상태를 반환한다.
		std::vector<ResourceHistoryState>& ResolveHistoryStates();
		// 실행 계획 검증 위치와 원인을 구조화된 오류로 조립한다.
		static PostProcessPlanError CreatePlanError(PostProcessPlanErrorCode code,
			size_t effectIndex, std::string message,
			size_t passIndex = PostProcessPlanError::noPassIndex);
		// 효과 선언을 검증하고 API 독립 실행 계획을 생성한다.
		static std::expected<PostProcessExecutionPlan, PostProcessPlanError> BuildExecutionPlan(
			const std::vector<const EffectRuntimeDefinition*>& effects);

	protected:
		PostProcess() = default;
		
		const std::vector<ShaderProgramDefinition>& GetShaderPrograms() const { return shaderPrograms; }
		const std::vector<PassRoute>& GetPassRoutes() const { return passRoutes; }
		const std::vector<ResourcePlan>& GetResourcePlans() const { return resourcePlans; }
		const ParameterData& GetParameterData(const PassRoute& route) const {
			return effectParameters[route.effectIndex];
		}

		// API별로 생성된 실행 리소스 수가 공통 패스 계획과 정확히 일치하는지 확인한다.
		bool IsPassCountCompatible(size_t passCount) const { return passCount == passRoutes.size(); }
		// 리소스 정의에 대응하는 실제 한 축의 해상도를 계산한다.
		static int ResolveResourceExtent(int fullExtent, const ResourcePlan& resource, bool width);
		// 출력 경로와 전체 화면 크기를 기준으로 패스 출력 크기를 계산한다.
		void ResolveOutputExtent(const PassRoute& route, int& width, int& height) const;
		// 리소스의 현재 read texture 인덱스를 반환한다.
		size_t ResolveResourceReadIndex(size_t resourceIndex, size_t transientIndex = 0) const;
		// 리소스의 다음 write texture 인덱스를 반환한다.
		size_t ResolveResourceWriteIndex(size_t resourceIndex, size_t transientIndex = 0) const;
		// history 리소스를 GPU에서 초기화해야 하는지 반환한다.
		bool NeedsHistoryInitialization(size_t resourceIndex) const;
		// history 리소스가 초기화된 상태임을 기록한다.
		void MarkHistoryInitialized(size_t resourceIndex);
		// 출력 경로가 history 리소스이면 read/write 인덱스를 전환한다.
		void AdvanceHistory(const PassRoute& route);
		// 새 후처리 프레임의 history 변경을 pending 상태에서 시작한다.
		void BeginHistoryFrame();
		// 검증을 마친 다른 후처리 객체와 API 독립 실행 계획을 교환한다.
		void SwapExecutionPlan(PostProcess& other) noexcept;
		// 준비된 실행 계획을 API별 후보 후처리 객체에 적용한다.
		void AdoptPreparedEffects(PreparedPostProcessEffects preparedEffects);

	public:
		virtual ~PostProcess() = default;

		PostProcess(const PostProcess&) = delete;
		PostProcess& operator=(const PostProcess&) = delete;

		bool HasEffects() const { return !shaderPrograms.empty(); }
		bool RequiresDepth() const { return depthRequired; }
		bool RequiresVelocity() const { return velocityRequired; }

		// GPU 대기나 API 리소스 생성 전에 효과 선언을 검증하고 실행 계획을 한 번 생성한다.
		static std::expected<PreparedPostProcessEffects, PostProcessPlanError> PrepareEffects(
			const std::vector<const EffectRuntimeDefinition*>& effects);
		// 활성 효과에 적용할 파라미터 갱신의 색인, 슬롯과 값 범위를 검증한다.
		bool ValidateParameterUpdates(std::span<const EffectParameterUpdate> updates) const;
		// 실행 계획을 다시 만들지 않고 활성 효과의 스칼라 파라미터 값을 갱신한다.
		bool UpdateParameters(std::span<const EffectParameterUpdate> updates);
		// GPU 실행이 확정된 프레임의 pending history를 committed 상태로 반영한다.
		void CommitHistoryFrame();
		// 실행되지 않은 프레임의 pending history 변경을 폐기한다.
		void DiscardHistoryFrame();
		// 다음 후처리 프레임에서 모든 temporal history를 초기 상태로 되돌린다.
		void ResetHistory();
		// 선택한 실행 계획은 유지하고 API별 GPU 리소스만 해제한다.
		virtual void ResetResources() = 0;
	};
}
