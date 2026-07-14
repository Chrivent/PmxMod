#pragma once

#include "Core/Animation/Model/Animation.h"

#include <string>

namespace Chrivent {
	class Model;

	// 애니메이션 트랙을 모델의 본, 모프와 IK 대상에 연결한다.
	class AnimationBinder {
		std::shared_ptr<Model> model;

		// 이름과 일치하는 모델 노드를 찾는다.
		std::shared_ptr<Node> FindNodeByName(const std::string& name) const;
		// 이름과 일치하는 IK 솔버를 찾는다.
		std::shared_ptr<IkSolver> FindIkSolverByName(const std::string& name) const;
		// 이름과 일치하는 모델 모프를 찾는다.
		std::shared_ptr<Morph> FindMorphByName(const std::string& name) const;

	public:
		explicit AnimationBinder(std::shared_ptr<Model> model) : model(std::move(model)) {}

		// 노드 이름과 일치하는 모델 노드를 트랙에 연결한다.
		void BindNodeTrack(NodeAnimationTrack& track, const std::string& name) const { track.node = FindNodeByName(name); }
		// IK 이름과 일치하는 IK 솔버를 트랙에 연결한다.
		void BindIkTrack(IkAnimationTrack& track, const std::string& name) const { track.ikSolver = FindIkSolverByName(name); }
		// 모프 이름과 일치하는 모델 모프를 트랙에 연결한다.
		void BindMorphTrack(MorphAnimationTrack& track, const std::string& name) const { track.morph = FindMorphByName(name); }
	};
}
