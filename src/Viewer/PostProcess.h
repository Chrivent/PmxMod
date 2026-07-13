#pragma once

#include "Viewer/Shader/ShaderPackage.h"

#include <vector>

namespace Chrivent {
	enum class PostProcessInputKind {
		SceneColor,
		SceneDepth,
		Resource
	};

	enum class PostProcessOutputKind {
		Present,
		Resource
	};

	struct PostProcessResourcePlan {
		EffectResourceLifetime lifetime = EffectResourceLifetime::Transient;
		EffectTextureFormat format = EffectTextureFormat::Rgba16Float;
		EffectPassResolution resolution = EffectPassResolution::Full;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct PostProcessPassInputRoute {
		uint32_t slot = 0;
		PostProcessInputKind kind = PostProcessInputKind::SceneColor;
		size_t resourceIndex = 0;
	};

	struct PostProcessPassRoute {
		std::vector<PostProcessPassInputRoute> inputs;
		PostProcessOutputKind outputKind = PostProcessOutputKind::Present;
		size_t outputResourceIndex = 0;
	};

	class PostProcess {
		std::vector<EffectPassDefinition> passDefinitions;
		std::vector<PostProcessPassRoute> passRoutes;
		std::vector<PostProcessResourcePlan> resourcePlans;
		bool depthRequired = false;

		// 선택한 effect들을 하나의 API 독립적인 실행 계획으로 변환한다.
		bool BuildExecutionPlan(const std::vector<const EffectDefinition*>& effects);

	protected:
		PostProcess() = default;

		// history ping-pong 인덱스를 다음 write target으로 전환한다.
		static size_t ResolveNextHistoryIndex(size_t currentIndex);
		// 리소스 정의에 대응하는 실제 한 축의 해상도를 계산한다.
		static int ResolveResourceExtent(int fullExtent, const PostProcessResourcePlan& resource, bool width);

		const std::vector<EffectPassDefinition>& ResolvePasses() const { return passDefinitions; }
		const std::vector<PostProcessPassRoute>& ResolvePassRoutes() const { return passRoutes; }
		const std::vector<PostProcessResourcePlan>& ResolveResourcePlans() const { return resourcePlans; }

	public:
		virtual ~PostProcess() = default;

		PostProcess(const PostProcess&) = delete;
		PostProcess& operator=(const PostProcess&) = delete;
		PostProcess(PostProcess&&) = delete;
		PostProcess& operator=(PostProcess&&) = delete;

		bool HasEffects() const { return !passDefinitions.empty(); }
		bool RequiresDepth() const { return depthRequired; }

		// 선택한 후처리 effect의 선언만으로 공통 실행 계획을 만든다.
		bool SetEffects(const std::vector<const EffectDefinition*>& effects);
		// 선택한 후처리 실행 계획만 비운다. GPU 리소스 해제는 API별 Reset이 담당한다.
		void ClearEffects();
		// 다음 후처리 프레임에서 모든 temporal history를 초기 상태로 되돌린다.
		virtual void ResetHistory() = 0;
		// API별 후처리 GPU 리소스를 해제한다.
		virtual void Reset() = 0;
	};
}
