#pragma once

#include "Animation.h"

#include <string>

namespace Chrivent {
	class Model;

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

		void BindNodeTrack(NodeAnimationTrack& track, const std::string& name) const { track.node = FindNodeByName(name); }
		void BindIkTrack(IkAnimationTrack& track, const std::string& name) const { track.ikSolver = FindIkSolverByName(name); }
		void BindMorphTrack(MorphAnimationTrack& track, const std::string& name) const { track.morph = FindMorphByName(name); }
	};
}
