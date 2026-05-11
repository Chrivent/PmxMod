#include "ModelAnimation.h"

#include "../Model.h"
#include "../Util.h"

#include <ranges>

void NodeAnimationKey::ApplyMotion(const VmdReader::VmdMotion& motion) {
	time = static_cast<int32_t>(motion.frame);
	translate = motion.translate * glm::vec3(1, 1, -1);
	const glm::quat q = motion.quaternion;
	const auto rot = Util::InvZ(glm::mat3_cast(q));
	rotate = glm::quat_cast(rot);
	Animation::AssignBezier(txBezier,
		motion.interpolation[0], motion.interpolation[8],
		motion.interpolation[4], motion.interpolation[12]);
	Animation::AssignBezier(tyBezier,
		motion.interpolation[1], motion.interpolation[9],
		motion.interpolation[5], motion.interpolation[13]);
	Animation::AssignBezier(tzBezier,
		motion.interpolation[2], motion.interpolation[10],
		motion.interpolation[6], motion.interpolation[14]);
	Animation::AssignBezier(rotBezier,
		motion.interpolation[3], motion.interpolation[11],
		motion.interpolation[7], motion.interpolation[15]);
}

bool ModelAnimation::Add(const VmdReader& vmd) {
	std::map<std::string, std::pair<std::shared_ptr<Node>, std::vector<NodeAnimationKey>>> nodeMap;
	for (auto& node : nodes) {
		if (node.first)
			nodeMap.emplace(node.first->name, std::move(node));
	}
	nodes.clear();
	for (const auto& motion : vmd.motions) {
		auto nodeName = Util::SjisToUtf8(motion.boneName);
		auto [findIt, inserted] = nodeMap.try_emplace(nodeName);
		auto& [first, second] = findIt->second;
		if (inserted) {
			auto it = std::ranges::find(model->nodes, nodeName,
				[](const std::shared_ptr<Node>& node) { return node->name; });
			first = it != model->nodes.end() ? *it : nullptr;
		}
		if (!first)
			continue;
		second.emplace_back().ApplyMotion(motion);
	}
	for (auto& val : nodeMap | std::views::values) {
		std::ranges::sort(val.second, {}, &NodeAnimationKey::time);
		nodes.insert(std::move(val));
	}
	std::map<std::string, std::pair<std::shared_ptr<IkSolver>, std::vector<IkAnimationKey>>> ikMap;
	for (auto& ik : iks) {
		if (ik.first) {
			if (const auto ikNodePtr = ik.first->ikNode.lock())
				ikMap.emplace(ikNodePtr->name, std::move(ik));
		}
	}
	iks.clear();
	for (const auto& ik : vmd.iks) {
		for (const auto& [name, enable] : ik.ikInfos) {
			auto ikName = Util::SjisToUtf8(name);
			auto [findIt, inserted] = ikMap.try_emplace(ikName);
			auto& [first, second] = findIt->second;
			if (inserted) {
				auto it = std::ranges::find(model->ikSolvers, ikName,
					[](const std::shared_ptr<IkSolver>& ikSolver){
						const auto ikNodePtr = ikSolver->ikNode.lock();
						return ikNodePtr ? ikNodePtr->name : std::string{};
					}
				);
				first = it != model->ikSolvers.end() ? *it : nullptr;
			}
			if (!first)
				continue;
			auto& [time, ikEnable] = second.emplace_back();
			time = static_cast<int32_t>(ik.frame);
			ikEnable = enable != 0;
		}
	}
	for (auto& val : ikMap | std::views::values) {
		std::ranges::sort(val.second, {}, &IkAnimationKey::time);
		iks.insert(std::move(val));
	}
	std::map<std::string, std::pair<std::shared_ptr<Morph>, std::vector<MorphAnimationKey>>> morphMap;
	for (auto& morph : morphs) {
		if (morph.first)
			morphMap.emplace(morph.first->name, std::move(morph));
	}
	morphs.clear();
	for (const auto& [blendShapeName, frame, weight] : vmd.morphs) {
		auto morphName = Util::SjisToUtf8(blendShapeName);
		auto [findIt, inserted] = morphMap.try_emplace(morphName);
		auto& [first, second] = findIt->second;
		if (inserted) {
			auto it = std::ranges::find(model->morphs, morphName, &Morph::name);
			first = it != model->morphs.end() ? std::shared_ptr<Morph>(model, it->get()) : nullptr;
		}
		if (!first)
			continue;
		auto& [time, morphWeight] = second.emplace_back();
		time = static_cast<int32_t>(frame);
		morphWeight = weight;
	}
	for (auto& val : morphMap | std::views::values) {
		std::ranges::sort(val.second, {}, &MorphAnimationKey::time);
		morphs.insert(std::move(val));
	}
	return true;
}

void ModelAnimation::Destroy() {
	model.reset();
	nodes.clear();
	iks.clear();
	morphs.clear();
}

void ModelAnimation::Evaluate(const float t, const float animWeight) const {
	for (const auto& [node, keys]: nodes) {
		if (!node)
			continue;
		if (keys.empty()) {
			node->animTranslate = glm::vec3(0);
			node->animRotate = glm::quat(1, 0, 0, 0);
			continue;
		}
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
			[](const NodeAnimationKey& k) { return static_cast<float>(k.time); });
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
			const float txX  = Animation::FindBezierX(normalizedTime, txBezier.first.x,  txBezier.second.x);
			const float tyX  = Animation::FindBezierX(normalizedTime, tyBezier.first.x,  tyBezier.second.x);
			const float tzX  = Animation::FindBezierX(normalizedTime, tzBezier.first.x,  tzBezier.second.x);
			const float rotX = Animation::FindBezierX(normalizedTime, rotBezier.first.x, rotBezier.second.x);
			const float txY  = Animation::Bezier(txX,  txBezier.first.y,  txBezier.second.y);
			const float tyY  = Animation::Bezier(tyX,  tyBezier.first.y,  tyBezier.second.y);
			const float tzY  = Animation::Bezier(tzX,  tzBezier.first.y,  tzBezier.second.y);
			const float rotY = Animation::Bezier(rotX, rotBezier.first.y, rotBezier.second.y);
			vt = glm::mix(prev.translate, translate, glm::vec3(txY, tyY, tzY));
			q  = glm::slerp(prev.rotate,   rotate,   rotY);
		}
		node->animTranslate = animWeight != 1.0f ? glm::mix(node->baseAnimTranslate, vt, animWeight) : vt;
		node->animRotate = animWeight != 1.0f ? glm::slerp(node->baseAnimRotate, q, animWeight) : q;
	}
	for (const auto& [ikSolver, keys] : iks) {
		if (!ikSolver)
			continue;
		if (keys.empty()) {
			ikSolver->enable = true;
			continue;
		}
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
			[](const IkAnimationKey& k) { return static_cast<float>(k.time); });
		const bool enable = it != keys.begin() ? (it - 1)->ikEnable : keys.begin()->ikEnable;
		ikSolver->enable = animWeight < 1.0f ? ikSolver->baseAnimEnable : enable;
	}
	for (const auto& [morph, keys] : morphs) {
		if (!morph)
			continue;
		if (keys.empty())
			continue;
		const auto it = std::ranges::upper_bound(keys, t, std::less{},
			[](const MorphAnimationKey& k) { return static_cast<float>(k.time); });
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

void ModelAnimation::SyncPhysics(const float t) const {
	model->SaveBaseAnimation();
	for (int i = 0; i < 30; i++) {
		model->BeginAnimation();
		Evaluate(t, static_cast<float>(1 + i) / 30.0f);
		model->UpdateMorphAnimation();
		model->UpdateNodeAnimation(false);
		model->UpdatePhysicsAnimation(1.0f / 30.0f);
		model->UpdateNodeAnimation(true);
	}
}
