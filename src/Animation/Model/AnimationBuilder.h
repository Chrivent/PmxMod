#pragma once

#include "Animation.h"
#include "../../Parser/VmdParser.h"

namespace Chrivent {
	class AnimationBuilder {
		AnimationInfo& info;

		// VMD 본 모션 키를 런타임 노드 애니메이션 키로 변환한다.
		static NodeAnimationKey CreateNodeAnimationKey(const VmdParser::VmdMotion& motion);
		// VMD 본 모션 키를 노드 애니메이션 트랙에 병합한다.
		void AddNodeAnimations(const VmdParser::VmdData& vmdData) const;
		// VMD IK 키를 IK 애니메이션 트랙에 병합한다.
		void AddIkAnimations(const VmdParser::VmdData& vmdData) const;
		// VMD 모프 키를 모프 애니메이션 트랙에 병합한다.
		void AddMorphAnimations(const VmdParser::VmdData& vmdData) const;

	public:
		explicit AnimationBuilder(AnimationInfo& info) : info(info) {}

		// VMD 데이터를 애니메이션 트랙에 추가한다.
		bool Add(const VmdParser::VmdData& vmdData) const;
	};
}
