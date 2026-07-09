#include "Viewer/PostProcess.h"

namespace Chrivent {
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
