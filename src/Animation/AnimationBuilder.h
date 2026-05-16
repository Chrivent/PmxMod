#pragma once

#include "Animation.h"
#include "../Reader/VmdReader.h"

namespace Chrivent {
	class AnimationBuilder {
		Animation& animation;

		// VMD bone motion keys are merged into node animation tracks.
		void AddNodeAnimations(const VmdReader& vmd) const;
		// VMD IK keys are merged into IK animation tracks.
		void AddIkAnimations(const VmdReader& vmd) const;
		// VMD morph keys are merged into morph animation tracks.
		void AddMorphAnimations(const VmdReader& vmd) const;

	public:
		explicit AnimationBuilder(Animation& animation) : animation(animation) {}

		// Adds VMD data to the animation tracks.
		bool Add(const VmdReader& vmd) const;
	};
}
