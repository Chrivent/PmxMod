#pragma once

#include "Animation.h"
#include "../Reader/VmdReader.h"

namespace Chrivent {
	class AnimationBuilder {
		Animation& animation;

		// VMD 본 모션 키를 노드 애니메이션 트랙에 병합한다.
		void AddNodeAnimations(const VmdReader& vmd) const;
		// VMD IK 키를 IK 애니메이션 트랙에 병합한다.
		void AddIkAnimations(const VmdReader& vmd) const;
		// VMD 모프 키를 모프 애니메이션 트랙에 병합한다.
		void AddMorphAnimations(const VmdReader& vmd) const;

	public:
		explicit AnimationBuilder(Animation& animation) : animation(animation) {}

		// VMD 데이터를 애니메이션 트랙에 추가한다.
		bool Add(const VmdReader& vmd) const;
	};
}
