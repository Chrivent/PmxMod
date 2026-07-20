#include "Core/Animation/Model/AnimationBuilder.h"

#include "Core/Animation/Model/AnimationBinder.h"
#include "Core/Animation/Model/AnimationTrackMap.h"
#include "Util.h"

namespace Chrivent {
	NodeAnimationKey AnimationBuilder::CreateNodeAnimationKey(const VmdParser::VmdMotion& motion) {
		NodeAnimationKey key{};
		key.frame = motion.frame;
		key.translate = motion.translate * glm::vec3(1, 1, -1);
		const glm::quat q = motion.quaternion;
		const auto rot = Util::InvZ(glm::mat3_cast(q));
		key.rotate = glm::quat_cast(rot);
		key.txBezier.Assign(motion.interpolation[0], motion.interpolation[8], motion.interpolation[4], motion.interpolation[12]);
		key.tyBezier.Assign(motion.interpolation[1], motion.interpolation[9], motion.interpolation[5], motion.interpolation[13]);
		key.tzBezier.Assign(motion.interpolation[2], motion.interpolation[10], motion.interpolation[6], motion.interpolation[14]);
		key.rotBezier.Assign(motion.interpolation[3], motion.interpolation[11], motion.interpolation[7], motion.interpolation[15]);
		return key;
	}

	void AnimationBuilder::AddNodeAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.motions.empty())
			return;
		const AnimationBinder binder(model);
		auto nodeMap = AnimationTrackMap::TakeNodeTrackMap(nodeTracks);
		for (const auto& motion : vmdData.motions) {
			auto nodeName = Util::SjisToUtf8(motion.boneName, sizeof(motion.boneName));
			auto [findIt, inserted] = nodeMap.try_emplace(nodeName);
			auto& [node, keys] = findIt->second;
			if (inserted)
				binder.BindNodeTrack(findIt->second, nodeName);
			if (!node)
				continue;
			keys.emplace_back(CreateNodeAnimationKey(motion));
		}
		AnimationTrackMap::FlushTrackMap(nodeMap, nodeTracks, &NodeAnimationKey::frame);
	}

	void AnimationBuilder::AddIkAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.iks.empty())
			return;
		const AnimationBinder binder(model);
		auto ikMap = AnimationTrackMap::TakeIkTrackMap(ikTracks);
		for (const auto& ik : vmdData.iks) {
			for (const auto& [name, enable] : ik.ikStates) {
				auto ikName = Util::SjisToUtf8(name, sizeof(name));
				auto [findIt, inserted] = ikMap.try_emplace(ikName);
				auto& [ikSolver, keys] = findIt->second;
				if (inserted)
					binder.BindIkTrack(findIt->second, ikName);
				if (!ikSolver)
					continue;
				auto& [frame, ikEnable] = keys.emplace_back();
				frame = ik.frame;
				ikEnable = enable != 0;
			}
		}
		AnimationTrackMap::FlushTrackMap(ikMap, ikTracks, &IkAnimationKey::frame);
	}

	void AnimationBuilder::AddMorphAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.morphs.empty())
			return;
		const AnimationBinder binder(model);
		auto morphMap = AnimationTrackMap::TakeMorphTrackMap(morphTracks);
		for (const auto& [blendShapeName, frame, weight] : vmdData.morphs) {
			auto morphName = Util::SjisToUtf8(blendShapeName, sizeof(blendShapeName));
			auto [findIt, inserted] = morphMap.try_emplace(morphName);
			auto& [morph, keys] = findIt->second;
			if (inserted)
				binder.BindMorphTrack(findIt->second, morphName);
			if (!morph)
				continue;
			auto& [keyFrame, morphWeight] = keys.emplace_back();
			keyFrame = frame;
			morphWeight = weight;
		}
		AnimationTrackMap::FlushTrackMap(morphMap, morphTracks, &MorphAnimationKey::frame);
	}

	void AnimationBuilder::Build(const VmdParser::VmdData& vmdData) {
		AddNodeAnimations(vmdData);
		AddIkAnimations(vmdData);
		AddMorphAnimations(vmdData);
	}

	std::unique_ptr<Animation> AnimationBuilder::TakeAnimation() {
		return std::make_unique<Animation>(
			std::move(nodeTracks), std::move(ikTracks), std::move(morphTracks));
	}
}
