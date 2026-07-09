#pragma once

#include "Viewer/Shader/ShaderPackage.h"

#include <cstddef>
#include <filesystem>
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
		static constexpr size_t targetCount = 3;
		static constexpr size_t focusHistoryCount = 2;
		std::vector<EffectDefinition> effectDefinitions;

		// 현재 pass가 읽고 쓸 ping-pong target 인덱스를 계산한다.
		static PostProcessPassRoute ResolvePingPongRoute(size_t passIndex, size_t passCount);
		// focus history ping-pong 인덱스를 다음 write target으로 전환한다.
		static int ResolveNextFocusHistoryIndex(int currentIndex);
		// focus history ping-pong 인덱스를 다음 write target으로 전환한다.
		static size_t ResolveNextFocusHistoryIndex(size_t currentIndex);
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

		// effect가 기본 DOF 효과인지 반환한다.
		static bool IsDepthOfFieldEffect(const EffectDefinition& effect);
		// DOF 자동 초점 갱신 pass에 사용할 focus-update 셰이더 경로를 반환한다.
		static std::filesystem::path ResolveFocusUpdateShaderPath(const EffectPassDefinition& pass);
		// UI에서 선택한 후처리 effect 정의를 내부에 복사한다.
		void SetEffects(const std::vector<const EffectDefinition*>& effects);
		// 선택한 후처리 effect 목록만 비운다. GPU 리소스 해제는 API별 Reset이 담당한다.
		void ClearEffects();
		// API별 후처리 GPU 리소스를 해제한다.
		virtual void Reset() = 0;
	};
}
