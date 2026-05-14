#include "Animation.h"

#include "AnimationHelper.h"
#include "../Model/ModelAnimator.h"
#include "../Model/ModelPose.h"
#include "../Util.h"

namespace Chrivent {
	using namespace AnimationHelper;
	
	void NodeAnimationKey::ApplyMotion(const VmdReader::VmdMotion& motion) {
		time = static_cast<int32_t>(motion.frame);
		translate = motion.translate * glm::vec3(1, 1, -1);
		const glm::quat q = motion.quaternion;
		const auto rot = Util::InvZ(glm::mat3_cast(q));
		rotate = glm::quat_cast(rot);
		txBezier.Assign(
			motion.interpolation[0], motion.interpolation[8],
			motion.interpolation[4], motion.interpolation[12]);
		tyBezier.Assign(
			motion.interpolation[1], motion.interpolation[9],
			motion.interpolation[5], motion.interpolation[13]);
		tzBezier.Assign(
			motion.interpolation[2], motion.interpolation[10],
			motion.interpolation[6], motion.interpolation[14]);
		rotBezier.Assign(
			motion.interpolation[3], motion.interpolation[11],
			motion.interpolation[7], motion.interpolation[15]);
	}

	std::shared_ptr<Node> Animation::FindNodeByName(const std::string& name) const {
		const auto it = std::ranges::find_if(model->nodes,
			[&name](const std::shared_ptr<Node>& node) {
				return node && node->name == name;
		});
		return it != model->nodes.end() ? *it : nullptr;
	}

	std::shared_ptr<IkSolver> Animation::FindIkSolverByName(const std::string& name) const {
		const auto it = std::ranges::find_if(model->ikSolvers,
			[&name](const std::shared_ptr<IkSolver>& solver) {
				if (!solver)
					return false;
				const auto ikNode = solver->ikNode.lock();
				return ikNode && ikNode->name == name;
		});
		return it != model->ikSolvers.end() ? *it : nullptr;
	}

	std::shared_ptr<Morph> Animation::FindMorphByName(const std::string& name) const {
		const auto it = std::ranges::find_if(model->morphs,
			[&name](const auto& morph) {
				return morph && morph->name == name;
		});
		if (it == model->morphs.end())
			return nullptr;
		return std::shared_ptr<Morph>(model, it->get());
	}

	void Animation::AddNodeAnimations(const VmdReader& vmd) {
		auto nodeMap = TakeNodeTrackMap(nodeTracks);
		for (const auto& motion : vmd.motions) {
			auto nodeName = Util::SjisToUtf8(motion.boneName);
			auto [findIt, inserted] = nodeMap.try_emplace(nodeName);
			auto& [node, keys] = findIt->second;
			if (inserted)
				node = FindNodeByName(nodeName);
			if (!node)
				continue;
			keys.emplace_back().ApplyMotion(motion);
		}
		FlushTrackMap(nodeMap, nodeTracks, &NodeAnimationKey::time);
	}

	void Animation::AddIkAnimations(const VmdReader& vmd) {
		auto ikMap = TakeIkTrackMap(ikTracks);
		for (const auto& ik : vmd.iks) {
			for (const auto& [name, enable] : ik.ikInfos) {
				auto ikName = Util::SjisToUtf8(name);
				auto [findIt, inserted] = ikMap.try_emplace(ikName);
				auto& [ikSolver, keys] = findIt->second;
				if (inserted)
					ikSolver = FindIkSolverByName(ikName);
				if (!ikSolver)
					continue;
				auto& [time, ikEnable] = keys.emplace_back();
				time = static_cast<int32_t>(ik.frame);
				ikEnable = enable != 0;
			}
		}
		FlushTrackMap(ikMap, ikTracks, &IkAnimationKey::time);
	}

	void Animation::AddMorphAnimations(const VmdReader& vmd) {
		auto morphMap = TakeMorphTrackMap(morphTracks);
		for (const auto& [blendShapeName, frame, weight] : vmd.morphs) {
			auto morphName = Util::SjisToUtf8(blendShapeName);
			auto [findIt, inserted] = morphMap.try_emplace(morphName);
			auto& [morph, keys] = findIt->second;
			if (inserted)
				morph = FindMorphByName(morphName);
			if (!morph)
				continue;
			auto& [time, morphWeight] = keys.emplace_back();
			time = static_cast<int32_t>(frame);
			morphWeight = weight;
		}
		FlushTrackMap(morphMap, morphTracks, &MorphAnimationKey::time);
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
			const auto it = FindUpperKey(keys, t);
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
			const auto it = FindUpperKey(keys, t);
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
			const auto it = FindUpperKey(keys, t);
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

	bool Animation::Add(const VmdReader& vmd) {
		AddNodeAnimations(vmd);
		AddIkAnimations(vmd);
		AddMorphAnimations(vmd);
		return true;
	}

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

	void Animation::SyncPhysics(const float t) const {
		const ModelAnimator animator(*model);
		animator.SaveBaseAnimation();
		for (int i = 0; i < 30; i++) {
			animator.BeginAnimation();
			Evaluate(t, static_cast<float>(1 + i) / 30.0f);
			animator.UpdateMorphAnimation();
			ModelPose pose(*model);
			pose.UpdateNodeAnimation(false);
			pose.UpdatePhysicsAnimation(1.0f / 30.0f);
			pose.UpdateNodeAnimation(true);
		}
	}
}
