#pragma once

#include "Core/Animation/Model/Animation.h"
#include "Core/Parser/VmdParser.h"

namespace Chrivent {
	class Model;

	class AnimationBuilder {
		std::vector<NodeAnimationTrack> nodeTracks;
		std::vector<IkAnimationTrack> ikTracks;
		std::vector<MorphAnimationTrack> morphTracks;
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
		// 완성된 애니메이션을 반환하고 내부 트랙을 비운다.
		std::unique_ptr<Animation> TakeAnimation();
	};
}
