#pragma once

#include "Core/Animation/Model/Animation.h"

#include <algorithm>
#include <map>
#include <ranges>
#include <string>

namespace Chrivent {
	// 애니메이션 트랙 이름과 런타임 인덱스의 대응 관계를 관리한다.
	class AnimationTrackMap {
		// 노드 애니메이션 트랙이 모델 노드에 연결되어 있는지 확인한다.
		static bool IsTrackBound(const NodeAnimationTrack& track) { return track.node != nullptr; }
		// IK 애니메이션 트랙이 IK 솔버에 연결되어 있는지 확인한다.
		static bool IsTrackBound(const IkAnimationTrack& track) { return track.ikSolver != nullptr; }
		// 모프 애니메이션 트랙이 모델 모프에 연결되어 있는지 확인한다.
		static bool IsTrackBound(const MorphAnimationTrack& track) { return track.morph != nullptr; }

	public:
		// 노드 트랙 목록을 노드 이름 기반 맵으로 옮긴다.
		static std::map<std::string, NodeAnimationTrack> TakeNodeTrackMap(std::vector<NodeAnimationTrack>& tracks);
		// IK 트랙 목록을 IK 노드 이름 기반 맵으로 옮긴다.
		static std::map<std::string, IkAnimationTrack> TakeIkTrackMap(std::vector<IkAnimationTrack>& tracks);
		// 모프 트랙 목록을 모프 이름 기반 맵으로 옮긴다.
		static std::map<std::string, MorphAnimationTrack> TakeMorphTrackMap(std::vector<MorphAnimationTrack>& tracks);

		// 이름 기반 트랙 맵에서 연결된 트랙만 시간순으로 정렬해 목록으로 옮긴다.
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
