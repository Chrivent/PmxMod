#include "Core/Animation/Model/AnimationBuilder.h"

#include "Core/Animation/AnimationKeySequence.h"
#include "Core/Model/Model.h"
#include "Core/Model/ModelCoordinateConverter.h"
#include "Core/Text/TextEncoding.h"

#include <ranges>

namespace Chrivent {
	Node* AnimationBuilder::FindNodeByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto& nodes = model->skeletonData.GetNodes();
		const auto it = std::ranges::find_if(nodes, [&name](const std::shared_ptr<Node>& node) {
			return node && node->name == name;
		});
		return it != nodes.end() ? it->get() : nullptr;
	}

	IkSolver* AnimationBuilder::FindIkSolverByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto& ikSolvers = model->skeletonData.GetIkSolvers();
		const auto it = std::ranges::find_if(ikSolvers, [&name](const std::shared_ptr<IkSolver>& solver) {
			if (!solver)
				return false;
			const auto ikNode = solver->ikNode.lock();
			return ikNode && ikNode->name == name;
		});
		return it != ikSolvers.end() ? it->get() : nullptr;
	}

	Morph* AnimationBuilder::FindMorphByName(const std::string& name) const {
		if (!model)
			return nullptr;
		const auto& morphs = model->morphData.GetMorphs();
		const auto it = std::ranges::find_if(morphs, [&name](const auto& morph) {
			return morph && morph->name == name;
		});
		return it != morphs.end() ? it->get() : nullptr;
	}

	template <typename TrackMap>
	std::vector<typename TrackMap::mapped_type> AnimationBuilder::TakeTracks(TrackMap& trackMap) {
		std::vector<typename TrackMap::mapped_type> tracks;
		tracks.reserve(trackMap.size());
		for (auto& track : trackMap | std::views::values) {
			if (!IsTrackBound(track))
				continue;
			AnimationKeySequence::SortAndKeepLastKeyPerFrame(track.keys);
			tracks.emplace_back(std::move(track));
		}
		trackMap.clear();
		return tracks;
	}

	NodeAnimationKey AnimationBuilder::CreateNodeAnimationKey(const VmdParser::VmdMotion& motion) {
		NodeAnimationKey key{};
		key.frame = motion.frame;
		key.translate = motion.translate * glm::vec3(1, 1, -1);
		const glm::quat q = glm::normalize(motion.quaternion);
		const auto rot = ModelCoordinateConverter::ConvertZAxis(glm::mat3_cast(q));
		key.rotate = glm::normalize(glm::quat_cast(rot));
		key.txBezier.Assign(motion.interpolation[0], motion.interpolation[8], motion.interpolation[4], motion.interpolation[12]);
		key.tyBezier.Assign(motion.interpolation[1], motion.interpolation[9], motion.interpolation[5], motion.interpolation[13]);
		key.tzBezier.Assign(motion.interpolation[2], motion.interpolation[10], motion.interpolation[6], motion.interpolation[14]);
		key.rotBezier.Assign(motion.interpolation[3], motion.interpolation[11], motion.interpolation[7], motion.interpolation[15]);
		return key;
	}

	void AnimationBuilder::AddNodeAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.motions.empty())
			return;
		for (const auto& motion : vmdData.motions) {
			auto nodeName = TextEncoding::ShiftJisToUtf8(motion.boneName, sizeof(motion.boneName));
			auto [findIt, inserted] = nodeTrackMap.try_emplace(nodeName);
			auto& [node, keys] = findIt->second;
			if (inserted)
				node = FindNodeByName(nodeName);
			if (!node)
				continue;
			keys.emplace_back(CreateNodeAnimationKey(motion));
		}
	}

	void AnimationBuilder::AddIkAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.iks.empty())
			return;
		for (const auto& ik : vmdData.iks) {
			for (const auto& [name, enable] : ik.ikStates) {
				auto ikName = TextEncoding::ShiftJisToUtf8(name, sizeof(name));
				auto [findIt, inserted] = ikTrackMap.try_emplace(ikName);
				auto& [ikSolver, keys] = findIt->second;
				if (inserted)
					ikSolver = FindIkSolverByName(ikName);
				if (!ikSolver)
					continue;
				auto& [frame, ikEnable] = keys.emplace_back();
				frame = ik.frame;
				ikEnable = enable != 0;
			}
		}
	}

	void AnimationBuilder::AddMorphAnimations(const VmdParser::VmdData& vmdData) {
		if (vmdData.morphs.empty())
			return;
		for (const auto& [blendShapeName, frame, weight] : vmdData.morphs) {
			auto morphName = TextEncoding::ShiftJisToUtf8(blendShapeName, sizeof(blendShapeName));
			auto [findIt, inserted] = morphTrackMap.try_emplace(morphName);
			auto& [morph, keys] = findIt->second;
			if (inserted)
				morph = FindMorphByName(morphName);
			if (!morph)
				continue;
			auto& [keyFrame, morphWeight] = keys.emplace_back();
			keyFrame = frame;
			morphWeight = weight;
		}
	}

	void AnimationBuilder::Build(const VmdParser::VmdData& vmdData) {
		AddNodeAnimations(vmdData);
		AddIkAnimations(vmdData);
		AddMorphAnimations(vmdData);
	}

	std::unique_ptr<Animation> AnimationBuilder::TakeAnimation() {
		auto nodeTracks = TakeTracks(nodeTrackMap);
		auto ikTracks = TakeTracks(ikTrackMap);
		auto morphTracks = TakeTracks(morphTrackMap);
		return std::make_unique<Animation>(
			model, std::move(nodeTracks), std::move(ikTracks), std::move(morphTracks));
	}
}
