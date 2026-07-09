#include "Viewer/PostProcess.h"

namespace Chrivent {
	bool PostProcess::IsDepthOfFieldEffect(const EffectDefinition& effect) {
		return effect.id == "depth-of-field";
	}

	std::filesystem::path PostProcess::ResolveFocusUpdateShaderPath(const EffectPassDefinition& pass) {
		return pass.shaderPath.parent_path() / "focus-update.hlsl";
	}

	PostProcessPassRoute PostProcess::ResolvePingPongRoute(const size_t passIndex, const size_t passCount) {
		PostProcessPassRoute route;
		route.lastPass = passIndex + 1 == passCount;
		route.pingPongIndex = passIndex % 2;
		route.sourceIndex = passIndex == 0 ? 0 : (passIndex - 1) % 2 + 1;
		route.targetIndex = route.pingPongIndex + 1;
		return route;
	}

	int PostProcess::ResolveNextFocusHistoryIndex(const int currentIndex) {
		return 1 - currentIndex;
	}

	size_t PostProcess::ResolveNextFocusHistoryIndex(const size_t currentIndex) {
		return 1 - currentIndex;
	}

	std::vector<const EffectDefinition*> PostProcess::ResolveEffectPointers() const {
		std::vector<const EffectDefinition*> effects;
		effects.reserve(effectDefinitions.size());
		for (const auto& effect : effectDefinitions)
			effects.push_back(&effect);
		return effects;
	}

	void PostProcess::SetEffects(const std::vector<const EffectDefinition*>& effects) {
		effectDefinitions.clear();
		effectDefinitions.reserve(effects.size());
		for (const auto* effect : effects) {
			if (effect != nullptr && !effect->passes.empty())
				effectDefinitions.push_back(*effect);
		}
	}

	void PostProcess::ClearEffects() {
		effectDefinitions.clear();
	}
}
