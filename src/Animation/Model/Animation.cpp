#include "Animation.h"

#include "../AnimationKeySearch.h"
#include "../../Model/Model.h"

namespace Chrivent {
	void Animation::Destroy() {
		info.model.reset();
		info.nodeTracks.clear();
		info.ikTracks.clear();
		info.morphTracks.clear();
	}

	uint32_t Animation::GetLastFrame() const {
		uint32_t lastFrame = 0;
		for (const auto& [node, keys] : info.nodeTracks) {
			if (!keys.empty())
				lastFrame = (std::max)(lastFrame, keys.back().frame);
		}
		for (const auto& [ikSolver, keys] : info.ikTracks) {
			if (!keys.empty())
				lastFrame = (std::max)(lastFrame, keys.back().frame);
		}
		for (const auto& [morph, keys] : info.morphTracks) {
			if (!keys.empty())
				lastFrame = (std::max)(lastFrame, keys.back().frame);
		}
		return lastFrame;
	}

	void Animation::Evaluate(const float t, const float animWeight) const {
		EvaluateNodes(t, animWeight);
		EvaluateIks(t, animWeight);
		EvaluateMorphs(t, animWeight);
	}
	
	void Animation::EvaluateNodes(const float t, const float animWeight) const {
		for (const auto& [node, keys]: info.nodeTracks) {
			if (!node)
				continue;
			if (keys.empty()) {
				node->GetInfo().animTranslate = glm::vec3(0);
				node->GetInfo().animRotate = glm::quat(1, 0, 0, 0);
				continue;
			}
			const auto it = AnimationKeySearch::FindUpperKey(keys, t);
			const auto& cur = it != keys.end() ? *it : keys.back();
			glm::vec3 vt = cur.translate;
			glm::quat q  = cur.rotate;
			if (it != keys.begin() && it != keys.end()) {
				const auto& prev = *(it - 1);
				const auto& [frame, translate, rotate,
					txBezier, tyBezier, tzBezier,
					rotBezier] = *it;
				const float prevFrame = prev.frame;
				const float nextFrame = frame;
				const float normalizedTime = (t - prevFrame) / (nextFrame - prevFrame);
				const float txY = txBezier.Evaluate(normalizedTime);
				const float tyY = tyBezier.Evaluate(normalizedTime);
				const float tzY = tzBezier.Evaluate(normalizedTime);
				const float rotY = rotBezier.Evaluate(normalizedTime);
				vt = glm::mix(prev.translate, translate, glm::vec3(txY, tyY, tzY));
				q  = glm::slerp(prev.rotate,   rotate,   rotY);
			}
			node->GetInfo().animTranslate = animWeight != 1.0f ? glm::mix(node->GetInfo().baseAnimTranslate, vt, animWeight) : vt;
			node->GetInfo().animRotate = animWeight != 1.0f ? glm::slerp(node->GetInfo().baseAnimRotate, q, animWeight) : q;
		}
	}

	void Animation::EvaluateIks(const float t, const float animWeight) const {
		for (const auto& [ikSolver, keys] : info.ikTracks) {
			if (!ikSolver)
				continue;
			if (keys.empty()) {
				ikSolver->GetInfo().enable = true;
				continue;
			}
			const auto it = AnimationKeySearch::FindUpperKey(keys, t);
			const bool enable = it != keys.begin() ? (it - 1)->ikEnable : keys.begin()->ikEnable;
			ikSolver->GetInfo().enable = animWeight < 1.0f ? ikSolver->GetInfo().baseAnimEnable : enable;
		}
	}

	void Animation::EvaluateMorphs(const float t, const float animWeight) const {
		for (const auto& [morph, keys] : info.morphTracks) {
			if (!morph)
				continue;
			if (keys.empty())
				continue;
			const auto it = AnimationKeySearch::FindUpperKey(keys, t);
			float weight = it != keys.end() ? it->morphWeight : keys.back().morphWeight;
			if (it != keys.begin() && it != keys.end()) {
				auto [frame0, weight0] = *(it - 1);
				auto [frame1, weight1] = *it;
				const float frame = (t - frame0) / static_cast<float>(frame1 - frame0);
				weight = (weight1 - weight0) * frame + weight0;
			}
			morph->weight = animWeight != 1.0f ? glm::mix(morph->saveAnimWeight, weight, animWeight) : weight;
		}
	}
}
