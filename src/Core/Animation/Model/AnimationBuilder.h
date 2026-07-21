#pragma once

#include "Core/Animation/Model/Animation.h"
#include "Core/Parser/VmdParser.h"

#include <map>
#include <string>

namespace Chrivent {
	class Model;

	// VMD 모션 데이터를 모델용 애니메이션 트랙으로 변환한다.
	class AnimationBuilder {
		std::map<std::string, NodeAnimationTrack> nodeTrackMap;
		std::map<std::string, IkAnimationTrack> ikTrackMap;
		std::map<std::string, MorphAnimationTrack> morphTrackMap;
		std::shared_ptr<Model> model;

		// 이름과 일치하는 모델 노드를 찾는다.
		Node* FindNodeByName(const std::string& name) const;
		// 이름과 일치하는 IK 솔버를 찾는다.
		IkSolver* FindIkSolverByName(const std::string& name) const;
		// 이름과 일치하는 모델 모프를 찾는다.
		Morph* FindMorphByName(const std::string& name) const;
		// 노드 애니메이션 트랙이 모델 노드에 연결되어 있는지 확인한다.
		static bool IsTrackBound(const NodeAnimationTrack& track) { return track.node != nullptr; }
		// IK 애니메이션 트랙이 IK 솔버에 연결되어 있는지 확인한다.
		static bool IsTrackBound(const IkAnimationTrack& track) { return track.ikSolver != nullptr; }
		// 모프 애니메이션 트랙이 모델 모프에 연결되어 있는지 확인한다.
		static bool IsTrackBound(const MorphAnimationTrack& track) { return track.morph != nullptr; }
		// 이름 기반 트랙 맵에서 연결된 트랙만 정렬해 목록으로 옮긴다.
		template <typename TrackMap>
		static std::vector<typename TrackMap::mapped_type> TakeTracks(TrackMap& trackMap);
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
