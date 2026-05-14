#include "AnimationHelper.h"

#include "../Model/Model.h"

namespace Chrivent::AnimationHelper {
	std::map<std::string, NodeAnimationTrack> TakeNodeTrackMap(std::vector<NodeAnimationTrack>& tracks) {
		std::map<std::string, NodeAnimationTrack> trackMap;
		for (auto& track : tracks) {
			if (track.node)
				trackMap.emplace(track.node->name, std::move(track));
		}
		tracks.clear();
		return trackMap;
	}

	std::map<std::string, IkAnimationTrack> TakeIkTrackMap(std::vector<IkAnimationTrack>& tracks) {
		std::map<std::string, IkAnimationTrack> trackMap;
		for (auto& track : tracks) {
			if (!track.ikSolver)
				continue;
			const auto ikNode = track.ikSolver->ikNode.lock();
			if (!ikNode)
				continue;
			trackMap.emplace(ikNode->name, std::move(track));
		}
		tracks.clear();
		return trackMap;
	}

	std::map<std::string, MorphAnimationTrack> TakeMorphTrackMap(std::vector<MorphAnimationTrack>& tracks) {
		std::map<std::string, MorphAnimationTrack> trackMap;
		for (auto& track : tracks) {
			if (track.morph)
				trackMap.emplace(track.morph->name, std::move(track));
		}
		tracks.clear();
		return trackMap;
	}
}
