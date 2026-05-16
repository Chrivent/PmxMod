#pragma once

#include "Animation.h"

#include <string>

namespace Chrivent {
	class AnimationBinder {
		const Animation& animation;

		std::shared_ptr<Node> FindNodeByName(const std::string& name) const;
		std::shared_ptr<IkSolver> FindIkSolverByName(const std::string& name) const;
		std::shared_ptr<Morph> FindMorphByName(const std::string& name) const;

	public:
		explicit AnimationBinder(const Animation& animation) : animation(animation) {}

		void BindNodeTrack(NodeAnimationTrack& track, const std::string& name) const { track.node = FindNodeByName(name); }
		void BindIkTrack(IkAnimationTrack& track, const std::string& name) const { track.ikSolver = FindIkSolverByName(name); }
		void BindMorphTrack(MorphAnimationTrack& track, const std::string& name) const { track.morph = FindMorphByName(name); }
	};
}
