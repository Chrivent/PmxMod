#include "Core/Animation/Model/Animation.h"

#include "Core/Animation/AnimationKeySequence.h"
#include "Core/Model/Model.h"

#include <utility>

namespace Chrivent {
	Animation::Animation(std::shared_ptr<Model> model, std::vector<NodeAnimationTrack> nodes,
		std::vector<IkAnimationTrack> iks, std::vector<MorphAnimationTrack> morphs)
		: targetModel(std::move(model)),
		targetModelRevision(targetModel ? targetModel->GetStructureRevision() : 0),
		nodeTracks(std::move(nodes)), ikTracks(std::move(iks)), morphTracks(std::move(morphs)) {
		const auto NormalizeTracks = []<typename Tracks>(Tracks& tracks) {
			for (auto& track : tracks)
				AnimationKeySequence::SortAndKeepLastKeyPerFrame(track.keys);
		};
		NormalizeTracks(nodeTracks);
		NormalizeTracks(ikTracks);
		NormalizeTracks(morphTracks);
	}

	uint32_t Animation::CalculateLastFrame() const {
		uint32_t lastFrame = 0;
		const auto IncludeTracks = [&lastFrame](const auto& tracks) {
			for (const auto& track : tracks) {
				if (!track.keys.empty())
					lastFrame = std::max(lastFrame, track.keys.back().frame);
			}
		};
		IncludeTracks(nodeTracks);
		IncludeTracks(ikTracks);
		IncludeTracks(morphTracks);
		return lastFrame;
	}

	void Animation::Evaluate(const float t, const float animWeight) const {
		if (!targetModel || targetModel->GetStructureRevision() != targetModelRevision)
			return;
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
			const auto it = AnimationKeySequence::FindUpperKey(keys, t);
			const auto& cur = it != keys.end() ? *it : keys.back();
			glm::vec3 vt = cur.translate;
			glm::quat q  = cur.rotate;
			if (it != keys.begin() && it != keys.end()) {
				const auto& prev = *std::prev(it);
				const auto& [frame, translate, rotate,
					txBezier, tyBezier, tzBezier, rotBezier] = *it;
				const float prevFrame = static_cast<float>(prev.frame);
				const float nextFrame = static_cast<float>(frame);
				const float normalizedTime = (t - prevFrame) / (nextFrame - prevFrame);
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
			const auto it = AnimationKeySequence::FindUpperKey(keys, t);
			const bool enable = it != keys.begin() ? std::prev(it)->ikEnable : keys.begin()->ikEnable;
			ikSolver->enable = animWeight < 1.0f ? ikSolver->baseAnimEnable : enable;
		}
	}

	void Animation::EvaluateMorphs(const float t, const float animWeight) const {
		for (const auto& [morph, keys] : morphTracks) {
			if (!morph)
				continue;
			if (keys.empty()) {
				morph->weight = 0;
				continue;
			}
			const auto it = AnimationKeySequence::FindUpperKey(keys, t);
			float weight = it != keys.end() ? it->morphWeight : keys.back().morphWeight;
			if (it != keys.begin() && it != keys.end()) {
				const auto [frame0, weight0] = *std::prev(it);
				const auto [frame1, weight1] = *it;
				const float frame = (t - static_cast<float>(frame0)) /
					(static_cast<float>(frame1) - static_cast<float>(frame0));
				weight = (weight1 - weight0) * frame + weight0;
			}
			morph->weight = animWeight != 1.0f ? glm::mix(morph->saveAnimWeight, weight, animWeight) : weight;
		}
	}
}
