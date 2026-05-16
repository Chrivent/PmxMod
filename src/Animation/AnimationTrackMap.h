#pragma once

#include "Animation.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <string>

namespace Chrivent::AnimationTrackMap {
	static bool IsTrackBound(const NodeAnimationTrack& track) { return track.node != nullptr; }
	static bool IsTrackBound(const IkAnimationTrack& track) { return track.ikSolver != nullptr; }
	static bool IsTrackBound(const MorphAnimationTrack& track) { return track.morph != nullptr; }

	std::map<std::string, NodeAnimationTrack> TakeNodeTrackMap(std::vector<NodeAnimationTrack>& tracks);
	std::map<std::string, IkAnimationTrack> TakeIkTrackMap(std::vector<IkAnimationTrack>& tracks);
	std::map<std::string, MorphAnimationTrack> TakeMorphTrackMap(std::vector<MorphAnimationTrack>& tracks);

	template <typename TrackMap, typename TrackList, typename TimeMember>
	static void FlushTrackMap(TrackMap& trackMap, TrackList& tracks, TimeMember timeMember) {
		tracks.clear();
		for (auto& track : trackMap | std::views::values) {
			if (!IsTrackBound(track))
				continue;
			std::ranges::sort(track.keys, {}, timeMember);
			tracks.emplace_back(std::move(track));
		}
	}
}
