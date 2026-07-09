#pragma once

#include "Viewer/Shader/ShaderPackage.h"

#include <cstddef>
#include <vector>

namespace Chrivent {
	class PostProcess {
	protected:
		static constexpr size_t targetCount = 3;
		static constexpr size_t focusHistoryCount = 2;
		std::vector<EffectDefinition> effectDefinitions;

		// 보관 중인 effect 정의를 API별 초기화 함수에 넘길 포인터 목록으로 만든다.
		std::vector<const EffectDefinition*> ResolveEffectPointers() const;

	public:
		PostProcess() = default;
		virtual ~PostProcess() = default;

		PostProcess(const PostProcess&) = delete;
		PostProcess& operator=(const PostProcess&) = delete;
		PostProcess(PostProcess&&) = delete;
		PostProcess& operator=(PostProcess&&) = delete;

		bool HasEffects() const { return !effectDefinitions.empty(); }

		// UI에서 선택한 후처리 effect 정의를 내부에 복사한다.
		void SetEffects(const std::vector<const EffectDefinition*>& effects);
		// 선택한 후처리 effect 목록만 비운다. GPU 리소스 해제는 API별 Reset이 담당한다.
		void ClearEffects();
		// API별 후처리 GPU 리소스를 해제한다.
		virtual void Reset() = 0;
	};
}
