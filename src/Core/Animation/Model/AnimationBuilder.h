#pragma once

#include "Animation.h"
#include "../../Parser/VmdParser.h"

namespace Chrivent {
	struct Model;

	class AnimationBuilder {
		AnimationInfo info;
		std::shared_ptr<Model> model;

		// VMD 본 모션 키를 런타임 노드 애니메이션 키로 변환한다.
		static NodeAnimationKey CreateNodeAnimationKey(const VmdParser::VmdMotion& motion);
		// VMD 본 모션 키를 노드 애니메이션 트랙에 병합한다.
		void AddNodeAnimations(const VmdParser::VmdData& vmdData);
		// VMD IK 키를 IK 애니메이션 트랙에 병합한다.
		void AddIkAnimations(const VmdParser::VmdData& vmdData);
		// VMD 모프 키를 모프 애니메이션 트랙에 병합한다.
		void AddMorphAnimations(const VmdParser::VmdData& vmdData);

	public:
		explicit AnimationBuilder(std::shared_ptr<Model> model) : model(std::move(model)) {}

		// VMD 데이터를 현재 애니메이션 트랙에 병합한다.
		void Build(const VmdParser::VmdData& vmdData);
		// 완성된 애니메이션 정보를 반환하고 내부 상태를 비운다.
		AnimationInfo TakeInfo();
	};
}
