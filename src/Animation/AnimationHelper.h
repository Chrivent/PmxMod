#pragma once

#include "Animation.h"

#include <map>
#include <ranges>

namespace AnimationHelper {
	// 노드 트랙이 실제 노드에 연결되어 있는지 확인한다.
	static bool IsTrackBound(const NodeAnimationTrack& track) { return track.node != nullptr; }
	// IK 트랙이 실제 IK 솔버에 연결되어 있는지 확인한다.
	static bool IsTrackBound(const IkAnimationTrack& track) { return track.ikSolver != nullptr; }
	// 모프 트랙이 실제 모프에 연결되어 있는지 확인한다.
	static bool IsTrackBound(const MorphAnimationTrack& track) { return track.morph != nullptr; }
	// 노드 트랙 목록을 노드 이름 기반 맵으로 옮긴다.
	std::map<std::string, NodeAnimationTrack> TakeNodeTrackMap(std::vector<NodeAnimationTrack>& tracks);
	// IK 트랙 목록을 IK 노드 이름 기반 맵으로 옮긴다.
	std::map<std::string, IkAnimationTrack> TakeIkTrackMap(std::vector<IkAnimationTrack>& tracks);
	// 모프 트랙 목록을 모프 이름 기반 맵으로 옮긴다.
	std::map<std::string, MorphAnimationTrack> TakeMorphTrackMap(std::vector<MorphAnimationTrack>& tracks);
	// 지정 시간보다 큰 첫 번째 키를 찾는다.
	template <typename Keys>
	static auto FindUpperKey(const Keys& keys, const float t) {
		return std::ranges::upper_bound(keys, t, std::less{}, [](const auto& key) {
			return static_cast<float>(key.time);
		});
	}
	// 트랙 맵의 바인딩된 트랙만 시간순으로 정렬해 트랙 목록으로 옮긴다.
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
