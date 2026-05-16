#include "Animation.h"

#include "AnimationKeySearch.h"
#include "../Model/Model.h"

namespace Chrivent {
	void Animation::Destroy() {
		model.reset();
		nodeTracks.clear();
		ikTracks.clear();
		morphTracks.clear();
	}

	void Animation::Evaluate(const float t, const float animWeight) const {
		EvaluateNodes(t, animWeight);
		EvaluateIks(t, animWeight);
		EvaluateMorphs(t, animWeight);
	}
	
	void Animation::EvaluateNodes(const float t, const float animWeight) const {
		for (const auto& [node, keys]: nodeTracks) {
			if (!node)
				continue;
			if (keys.empty()) {
				node->animTranslate = glm::vec3(0);
				node->animRotate = glm::quat(1, 0, 0, 0);
				continue;
			}
			const auto it = AnimationKeySearch::FindUpperKey(keys, t);
			const auto& cur = it != keys.end() ? *it : keys.back();
			glm::vec3 vt = cur.translate;
			glm::quat q  = cur.rotate;
			if (it != keys.begin() && it != keys.end()) {
				const auto& prev = *(it - 1);
				const auto& [time, translate, rotate,
					txBezier, tyBezier, tzBezier,
					rotBezier] = *it;
				const auto timeRange = static_cast<float>(time - prev.time);
				const float normalizedTime = (t - static_cast<float>(prev.time)) / timeRange;
				const float txY = txBezier.Evaluate(normalizedTime);
				const float tyY = tyBezier.Evaluate(normalizedTime);
				const float tzY = tzBezier.Evaluate(normalizedTime);
				const float rotY = rotBezier.Evaluate(normalizedTime);
				vt = glm::mix(prev.translate, translate, glm::vec3(txY, tyY, tzY));
				q  = glm::slerp(prev.rotate,   rotate,   rotY);
			}
			node->animTranslate = animWeight != 1.0f ? glm::mix(node->baseAnimTranslate, vt, animWeight) : vt;
			node->animRotate = animWeight != 1.0f ? glm::slerp(node->baseAnimRotate, q, animWeight) : q;
		}
	}

	void Animation::EvaluateIks(const float t, const float animWeight) const {
		for (const auto& [ikSolver, keys] : ikTracks) {
			if (!ikSolver)
				continue;
			if (keys.empty()) {
				ikSolver->enable = true;
				continue;
			}
			const auto it = AnimationKeySearch::FindUpperKey(keys, t);
			const bool enable = it != keys.begin() ? (it - 1)->ikEnable : keys.begin()->ikEnable;
			ikSolver->enable = animWeight < 1.0f ? ikSolver->baseAnimEnable : enable;
		}
	}

	void Animation::EvaluateMorphs(const float t, const float animWeight) const {
		for (const auto& [morph, keys] : morphTracks) {
			if (!morph)
				continue;
			if (keys.empty())
				continue;
			const auto it = AnimationKeySearch::FindUpperKey(keys, t);
			float weight = it != keys.end() ? it->morphWeight : keys.back().morphWeight;
			if (it != keys.begin() && it != keys.end()) {
				auto [time0, weight0] = *(it - 1);
				auto [time1, weight1] = *it;
				const float time = (t - static_cast<float>(time0)) / static_cast<float>(time1 - time0);
				weight = (weight1 - weight0) * time + weight0;
			}
			morph->weight = animWeight != 1.0f ? glm::mix(morph->saveAnimWeight, weight, animWeight) : weight;
		}
	}
}
