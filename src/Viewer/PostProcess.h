#pragma once

#include "Viewer/Shader/ShaderPackage.h"

#include <optional>
#include <vector>

namespace Chrivent {
	struct PostProcessPassRoute {
		bool lastPass = false;
		size_t sourceIndex = 0;
		size_t targetIndex = 0;
		size_t pingPongIndex = 0;
	};

	class PostProcess {
	protected:
		PostProcess() = default;

		static constexpr size_t targetCount = 3;
		static constexpr size_t historyTargetCount = 2;
		std::vector<EffectPassDefinition> passDefinitions;
		std::optional<EffectPassDefinition> historyPassDefinition;
		bool depthRequired = false;

		// 현재 pass가 읽고 쓸 ping-pong target 인덱스를 계산한다.
		static PostProcessPassRoute ResolvePingPongRoute(size_t passIndex, size_t passCount);
		// history ping-pong 인덱스를 다음 write target으로 전환한다.
		static size_t ResolveNextHistoryIndex(size_t currentIndex);

		const std::vector<EffectPassDefinition>& ResolvePasses() const { return passDefinitions; }
		const EffectPassDefinition* ResolveHistoryPass() const {
			return historyPassDefinition ? &*historyPassDefinition : nullptr;
		}

	public:
		virtual ~PostProcess() = default;

		PostProcess(const PostProcess&) = delete;
		PostProcess& operator=(const PostProcess&) = delete;
		PostProcess(PostProcess&&) = delete;
		PostProcess& operator=(PostProcess&&) = delete;

		bool HasEffects() const { return !passDefinitions.empty(); }
		bool RequiresDepth() const { return depthRequired; }

		// 선택한 후처리 effect를 실행 순서대로 펼쳐 공통 실행 계획을 만든다.
		bool SetEffects(const std::vector<const EffectDefinition*>& effects);
		// 선택한 후처리 실행 계획만 비운다. GPU 리소스 해제는 API별 Reset이 담당한다.
		void ClearEffects();
		// 다음 후처리 프레임에서 temporal history를 초기 상태로 되돌린다.
		virtual void ResetHistory() = 0;
		// API별 후처리 GPU 리소스를 해제한다.
		virtual void Reset() = 0;
	};
}
