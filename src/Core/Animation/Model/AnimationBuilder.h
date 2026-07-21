#pragma once

#include "Core/Animation/Model/Animation.h"
#include "Core/Parser/VmdParser.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Chrivent {
	class Model;

	// VMD 모션 데이터를 모델용 애니메이션 트랙으로 변환한다.
	class AnimationBuilder {
		std::map<std::string, std::vector<NodeAnimationKey>> nodeKeysByName;
		std::map<std::string, std::vector<IkAnimationKey>> ikKeysByName;
		std::map<std::string, std::vector<MorphAnimationKey>> morphKeysByName;
		std::map<std::string, std::string> decodedNames;
		std::shared_ptr<Model> model;

		// 고정 길이 Shift-JIS 이름을 한 번만 UTF-8로 변환해 보관한다.
		const std::string& ResolveName(const char* encodedName, std::size_t size);
		// 이름별 키를 현재 모델 대상에 연결하고 정렬된 트랙 목록으로 옮긴다.
		template <typename Track, typename KeyMap, typename Resolver>
		static std::vector<Track> TakeTracks(KeyMap& keysByName, const Resolver& resolveTarget);
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
