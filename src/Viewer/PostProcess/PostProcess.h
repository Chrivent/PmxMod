#pragma once

#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Shader/ShaderPackage.h"

#include <vector>

namespace Chrivent {
	enum class PostProcessInputKind {
		SceneColor,
		SceneDepth,
		SceneVelocity,
		Resource
	};

	enum class PostProcessOutputKind {
		Present,
		Resource
	};

	// 효과 리소스 하나의 수명과 형식 및 실제 출력 크기를 나타낸다.
	struct PostProcessResourcePlan {
		EffectResourceLifetime lifetime = EffectResourceLifetime::Transient;
		EffectTextureFormat format = EffectTextureFormat::Rgba16Float;
		EffectPassResolution resolution = EffectPassResolution::Full;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	// 패스의 texture 슬롯이 참조할 장면 또는 효과 리소스를 나타낸다.
	struct PostProcessPassInputRoute {
		uint32_t slot = 0;
		PostProcessInputKind kind = PostProcessInputKind::SceneColor;
		size_t resourceIndex = 0;
	};

	// 한 효과가 b1 상수 버퍼로 전달할 스칼라 파라미터 값을 보관한다.
	struct PostProcessParameterData {
		float values[PostProcessInputLayout::maxParameterCount]{};
	};

	// 후처리 패스 하나의 입력 경로와 출력 대상을 나타낸다.
	struct PostProcessPassRoute {
		std::vector<PostProcessPassInputRoute> inputs;
		PostProcessParameterData parameters;
		PostProcessOutputKind outputKind = PostProcessOutputKind::Present;
		size_t outputResourceIndex = 0;
	};
	
	// history 리소스의 현재 읽기 인덱스와 초기화 여부를 기록한다.
	struct ResourceHistoryState {
		size_t readIndex = 0;
		bool initialized = false;
	};

	// 패키지 효과를 API 독립적인 후처리 실행 계획으로 변환하고 상태를 관리한다.
	class PostProcess {
		std::vector<EffectPassDefinition> passDefinitions;
		std::vector<PostProcessPassRoute> passRoutes;
		std::vector<PostProcessResourcePlan> resourcePlans;
		std::vector<ResourceHistoryState> resourceHistoryStates;
		std::vector<ResourceHistoryState> pendingResourceHistoryStates;
		bool depthRequired = false;
		bool velocityRequired = false;
		bool historyFramePending = false;

		// 선택한 effect들을 하나의 API 독립적인 실행 계획으로 변환한다.
		bool BuildExecutionPlan(const std::vector<const EffectDefinition*>& effects);
		// history ping-pong 인덱스를 다음 write target으로 전환한다.
		static size_t ResolveNextHistoryIndex(size_t currentIndex);
		// 현재 패스 기록에서 사용할 committed 또는 pending history 상태를 반환한다.
		const std::vector<ResourceHistoryState>& ResolveHistoryStates() const;
		// 현재 패스 기록에서 수정할 committed 또는 pending history 상태를 반환한다.
		std::vector<ResourceHistoryState>& ResolveHistoryStates();

	protected:
		PostProcess() = default;

		// 리소스 정의에 대응하는 실제 한 축의 해상도를 계산한다.
		static int ResolveResourceExtent(int fullExtent, const PostProcessResourcePlan& resource, bool width);
		// 출력 경로와 전체 화면 크기를 기준으로 패스 출력 크기를 계산한다.
		void ResolveOutputExtent(const PostProcessPassRoute& route, int& width, int& height) const;
		// 리소스의 현재 read texture 인덱스를 반환한다.
		size_t ResolveResourceReadIndex(size_t resourceIndex, size_t transientIndex = 0) const;
		// 리소스의 다음 write texture 인덱스를 반환한다.
		size_t ResolveResourceWriteIndex(size_t resourceIndex, size_t transientIndex = 0) const;
		// history 리소스를 GPU에서 초기화해야 하는지 반환한다.
		bool NeedsHistoryInitialization(size_t resourceIndex) const;
		// history 리소스가 초기화된 상태임을 기록한다.
		void MarkHistoryInitialized(size_t resourceIndex);
		// 출력 경로가 history 리소스이면 read/write 인덱스를 전환한다.
		void AdvanceHistory(const PostProcessPassRoute& route);
		// 새 후처리 프레임의 history 변경을 pending 상태에서 시작한다.
		void BeginHistoryFrame();
		// 검증을 마친 다른 후처리 객체와 API 독립 실행 계획을 교환한다.
		void SwapExecutionPlan(PostProcess& other) noexcept;

		const std::vector<EffectPassDefinition>& ResolvePasses() const { return passDefinitions; }
		const std::vector<PostProcessPassRoute>& ResolvePassRoutes() const { return passRoutes; }
		const std::vector<PostProcessResourcePlan>& ResolveResourcePlans() const { return resourcePlans; }

		// 선택한 후처리 effect의 선언만으로 공통 실행 계획을 만든다.
		bool SetEffects(const std::vector<const EffectDefinition*>& effects);
		// 선택한 후처리 실행 계획만 비운다. GPU 리소스 해제는 API별 ResetResources가 담당한다.
		void ClearEffects();

	public:
		virtual ~PostProcess() = default;

		PostProcess(const PostProcess&) = delete;
		PostProcess& operator=(const PostProcess&) = delete;
		PostProcess(PostProcess&&) = delete;
		PostProcess& operator=(PostProcess&&) = delete;

		bool HasEffects() const { return !passDefinitions.empty(); }
		bool RequiresDepth() const { return depthRequired; }
		bool RequiresVelocity() const { return velocityRequired; }

		// GPU 실행이 확정된 프레임의 pending history를 committed 상태로 반영한다.
		void CommitHistoryFrame();
		// 실행되지 않은 프레임의 pending history 변경을 폐기한다.
		void DiscardHistoryFrame();
		// 다음 후처리 프레임에서 모든 temporal history를 초기 상태로 되돌린다.
		void ResetHistory();
		// API별 GPU 리소스와 선택한 실행 계획을 함께 해제한다.
		void Clear();
		// 선택한 실행 계획은 유지하고 API별 GPU 리소스만 해제한다.
		virtual void ResetResources() = 0;
	};
}
