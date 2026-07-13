#include "Viewer/PostProcess.h"

#include <algorithm>

namespace Chrivent {
	std::vector<PostProcessPassRoute> PostProcess::BuildPassRoutes(
		const std::vector<EffectPassDefinition>& passes, const std::vector<size_t>& effectIndices) {
		std::vector<PostProcessPassRoute> routes;
		routes.reserve(passes.size());
		size_t sourceIndex = sceneTargetIndex;
		size_t effectSourceIndex = sceneTargetIndex;
		size_t fullWriteIndex = 0;
		size_t halfWriteIndex = 0;
		for (size_t passIndex = 0; passIndex < passes.size(); passIndex++) {
			if (passIndex == 0 || effectIndices[passIndex] != effectIndices[passIndex - 1])
				effectSourceIndex = sourceIndex;
			PostProcessPassRoute route{
				.lastPass = passIndex + 1 == passes.size(),
				.sourceIndex = sourceIndex,
				.effectSourceIndex = effectSourceIndex,
				.resolution = passes[passIndex].resolution
			};
			if (!route.lastPass) {
				size_t& writeIndex = route.resolution == EffectPassResolution::Half
					? halfWriteIndex : fullWriteIndex;
				const size_t targetOffset = route.resolution == EffectPassResolution::Half
					? halfTargetOffset : fullTargetOffset;
				route.targetIndex = targetOffset + writeIndex;
				if (route.targetIndex == sourceIndex) {
					writeIndex = ResolveNextHistoryIndex(writeIndex);
					route.targetIndex = targetOffset + writeIndex;
				}
				writeIndex = ResolveNextHistoryIndex(writeIndex);
				sourceIndex = route.targetIndex;
			}
			routes.emplace_back(route);
		}
		return routes;
	}

	size_t PostProcess::ResolveNextHistoryIndex(const size_t currentIndex) {
		return 1 - currentIndex;
	}

	int PostProcess::ResolveTargetExtent(const int fullExtent, const size_t targetIndex) {
		return targetIndex >= halfTargetOffset ? std::max(1, (fullExtent + 1) / 2) : fullExtent;
	}

	bool PostProcess::SetEffects(const std::vector<const EffectDefinition*>& effects) {
		std::vector<EffectPassDefinition> passes;
		std::vector<size_t> effectIndices;
		std::optional<EffectPassDefinition> historyPass;
		bool requiresDepth = false;
		for (size_t effectIndex = 0; effectIndex < effects.size(); effectIndex++) {
			const auto* effect = effects[effectIndex];
			if (effect == nullptr || effect->passes.empty())
				continue;
			passes.insert(passes.end(), effect->passes.begin(), effect->passes.end());
			effectIndices.insert(effectIndices.end(), effect->passes.size(), effectIndex);
			requiresDepth = requiresDepth || effect->requiresDepth;
			if (!effect->historyPass)
				continue;
			if (historyPass)
				return false;
			historyPass = effect->historyPass;
		}
		if (!passes.empty() && passes.back().resolution != EffectPassResolution::Full)
			return false;
		passDefinitions = std::move(passes);
		passRoutes = BuildPassRoutes(passDefinitions, effectIndices);
		historyPassDefinition = std::move(historyPass);
		depthRequired = requiresDepth;
		return true;
	}

	void PostProcess::ClearEffects() {
		passDefinitions.clear();
		passRoutes.clear();
		historyPassDefinition.reset();
		depthRequired = false;
	}
}
