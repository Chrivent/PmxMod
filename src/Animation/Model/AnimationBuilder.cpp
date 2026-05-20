#include "AnimationBuilder.h"

#include "AnimationBinder.h"
#include "AnimationTrackMap.h"
#include "../../Util.h"

namespace Chrivent {
	NodeAnimationKey AnimationBuilder::CreateNodeAnimationKey(const VmdParser::VmdMotion& motion) {
		NodeAnimationKey key{};
		key.time = static_cast<int32_t>(motion.frame);
		key.translate = motion.translate * glm::vec3(1, 1, -1);
		const glm::quat q = motion.quaternion;
		const auto rot = Util::InvZ(glm::mat3_cast(q));
		key.rotate = glm::quat_cast(rot);
		key.txBezier.Assign(
			motion.interpolation[0], motion.interpolation[8],
			motion.interpolation[4], motion.interpolation[12]);
		key.tyBezier.Assign(
			motion.interpolation[1], motion.interpolation[9],
			motion.interpolation[5], motion.interpolation[13]);
		key.tzBezier.Assign(
			motion.interpolation[2], motion.interpolation[10],
			motion.interpolation[6], motion.interpolation[14]);
		key.rotBezier.Assign(
			motion.interpolation[3], motion.interpolation[11],
			motion.interpolation[7], motion.interpolation[15]);
		return key;
	}

	void AnimationBuilder::AddNodeAnimations(const VmdParser::VmdData& vmdData) const {
		const AnimationBinder binder(info);
		auto& nodeTracks = info.nodeTracks;
		auto nodeMap = AnimationTrackMap::TakeNodeTrackMap(nodeTracks);
		for (const auto& motion : vmdData.motions) {
			auto nodeName = Util::SjisToUtf8(motion.boneName);
			auto [findIt, inserted] = nodeMap.try_emplace(nodeName);
			auto& [node, keys] = findIt->second;
			if (inserted)
				binder.BindNodeTrack(findIt->second, nodeName);
			if (!node)
				continue;
			keys.emplace_back(CreateNodeAnimationKey(motion));
		}
		AnimationTrackMap::FlushTrackMap(nodeMap, nodeTracks, &NodeAnimationKey::time);
	}

	void AnimationBuilder::AddIkAnimations(const VmdParser::VmdData& vmdData) const {
		const AnimationBinder binder(info);
		auto& ikTracks = info.ikTracks;
		auto ikMap = AnimationTrackMap::TakeIkTrackMap(ikTracks);
		for (const auto& ik : vmdData.iks) {
			for (const auto& [name, enable] : ik.ikInfos) {
				auto ikName = Util::SjisToUtf8(name);
				auto [findIt, inserted] = ikMap.try_emplace(ikName);
				auto& [ikSolver, keys] = findIt->second;
				if (inserted)
					binder.BindIkTrack(findIt->second, ikName);
				if (!ikSolver)
					continue;
				auto& [time, ikEnable] = keys.emplace_back();
				time = static_cast<int32_t>(ik.frame);
				ikEnable = enable != 0;
			}
		}
		AnimationTrackMap::FlushTrackMap(ikMap, ikTracks, &IkAnimationKey::time);
	}

	void AnimationBuilder::AddMorphAnimations(const VmdParser::VmdData& vmdData) const {
		const AnimationBinder binder(info);
		auto& morphTracks = info.morphTracks;
		auto morphMap = AnimationTrackMap::TakeMorphTrackMap(morphTracks);
		for (const auto& [blendShapeName, frame, weight] : vmdData.morphs) {
			auto morphName = Util::SjisToUtf8(blendShapeName);
			auto [findIt, inserted] = morphMap.try_emplace(morphName);
			auto& [morph, keys] = findIt->second;
			if (inserted)
				binder.BindMorphTrack(findIt->second, morphName);
			if (!morph)
				continue;
			auto& [time, morphWeight] = keys.emplace_back();
			time = static_cast<int32_t>(frame);
			morphWeight = weight;
		}
		AnimationTrackMap::FlushTrackMap(morphMap, morphTracks, &MorphAnimationKey::time);
	}

	bool AnimationBuilder::Add(const VmdParser::VmdData& vmdData) const {
		AddNodeAnimations(vmdData);
		AddIkAnimations(vmdData);
		AddMorphAnimations(vmdData);
		return true;
	}
}
