#pragma once

#include "Animation.h"

#include <string>

namespace Chrivent {
	class AnimationBinder {
		AnimationInfo& info;

		// 이름과 일치하는 모델 노드를 찾는다.
		std::shared_ptr<Node> FindNodeByName(const std::string& name) const;
		// 이름과 일치하는 IK 솔버를 찾는다.
		std::shared_ptr<IkSolver> FindIkSolverByName(const std::string& name) const;
		// 이름과 일치하는 모델 모프를 찾는다.
		std::shared_ptr<Morph> FindMorphByName(const std::string& name) const;

	public:
		explicit AnimationBinder(AnimationInfo& info) : info(info) {}

		// 노드 트랙을 이름과 일치하는 모델 노드에 연결한다.
		void BindNodeTrack(NodeAnimationTrack& track, const std::string& name) const { track.node = FindNodeByName(name); }
		// IK 트랙을 이름과 일치하는 IK 솔버에 연결한다.
		void BindIkTrack(IkAnimationTrack& track, const std::string& name) const { track.ikSolver = FindIkSolverByName(name); }
		// 모프 트랙을 이름과 일치하는 모델 모프에 연결한다.
		void BindMorphTrack(MorphAnimationTrack& track, const std::string& name) const { track.morph = FindMorphByName(name); }
	};
}
