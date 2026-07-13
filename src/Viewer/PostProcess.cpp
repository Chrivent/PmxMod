#include "Viewer/PostProcess.h"

namespace Chrivent {
	PostProcessPassRoute PostProcess::ResolvePingPongRoute(const size_t passIndex, const size_t passCount) {
		PostProcessPassRoute route;
		route.lastPass = passIndex + 1 == passCount;
		route.pingPongIndex = passIndex % 2;
		route.sourceIndex = passIndex == 0 ? 0 : (passIndex - 1) % 2 + 1;
		route.targetIndex = route.pingPongIndex + 1;
		return route;
	}

	size_t PostProcess::ResolveNextHistoryIndex(const size_t currentIndex) {
		return 1 - currentIndex;
	}

	bool PostProcess::SetEffects(const std::vector<const EffectDefinition*>& effects) {
		std::vector<EffectPassDefinition> passes;
		std::optional<EffectPassDefinition> historyPass;
		bool requiresDepth = false;
		for (const auto* effect : effects) {
			if (effect == nullptr || effect->passes.empty())
				continue;
			passes.insert(passes.end(), effect->passes.begin(), effect->passes.end());
			requiresDepth = requiresDepth || effect->requiresDepth;
			if (!effect->historyPass)
				continue;
			if (historyPass)
				return false;
			historyPass = effect->historyPass;
		}
		passDefinitions = std::move(passes);
		historyPassDefinition = std::move(historyPass);
		depthRequired = requiresDepth;
		return true;
	}

	void PostProcess::ClearEffects() {
		passDefinitions.clear();
		historyPassDefinition.reset();
		depthRequired = false;
	}
}
