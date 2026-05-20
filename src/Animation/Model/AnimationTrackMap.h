#pragma once

#include "Animation.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <string>

namespace Chrivent {
	class AnimationTrackMap {
		static bool IsTrackBound(const NodeAnimationTrack& track) { return track.node != nullptr; }
		static bool IsTrackBound(const IkAnimationTrack& track) { return track.ikSolver != nullptr; }
		static bool IsTrackBound(const MorphAnimationTrack& track) { return track.morph != nullptr; }

	public:
		// 노드 트랙 목록을 노드 이름 기반 맵으로 옮긴다.
		static std::map<std::string, NodeAnimationTrack> TakeNodeTrackMap(std::vector<NodeAnimationTrack>& tracks);
		// IK 트랙 목록을 IK 노드 이름 기반 맵으로 옮긴다.
		static std::map<std::string, IkAnimationTrack> TakeIkTrackMap(std::vector<IkAnimationTrack>& tracks);
		// 모프 트랙 목록을 모프 이름 기반 맵으로 옮긴다.
		static std::map<std::string, MorphAnimationTrack> TakeMorphTrackMap(std::vector<MorphAnimationTrack>& tracks);

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
	};
}
