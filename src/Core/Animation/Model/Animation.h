#pragma once

#include "Core/Animation/Bezier.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	struct Morph;
	class Node;
	class IkSolver;

	// 한 프레임의 본 변환과 보간 곡선을 보관한다.
	struct NodeAnimationKey {
		uint32_t	frame = 0;
		glm::vec3	translate;
		glm::quat	rotate;
		Bezier		txBezier;
		Bezier		tyBezier;
		Bezier		tzBezier;
		Bezier		rotBezier;
	};

	// 한 프레임의 모프 가중치를 보관한다.
	struct MorphAnimationKey {
		uint32_t	frame = 0;
		float	morphWeight;
	};

	// 한 프레임의 IK 표시와 활성 상태를 보관한다.
	struct IkAnimationKey {
		uint32_t	frame = 0;
		bool	ikEnable;
	};

	// 본 하나에 적용할 변환 키 목록을 보관한다.
	struct NodeAnimationTrack {
		std::shared_ptr<Node> node;
		std::vector<NodeAnimationKey> keys;
	};

	// IK 대상 하나에 적용할 활성 키 목록을 보관한다.
	struct IkAnimationTrack {
		std::shared_ptr<IkSolver> ikSolver;
		std::vector<IkAnimationKey> keys;
	};

	// 모프 하나에 적용할 가중치 키 목록을 보관한다.
	struct MorphAnimationTrack {
		std::shared_ptr<Morph> morph;
		std::vector<MorphAnimationKey> keys;
	};

	// 모델의 본, 모프와 IK 트랙을 시간에 따라 평가한다.
	class Animation {
		// 지정 시간의 노드 애니메이션을 평가한다.
		void EvaluateNodes(float t, float animWeight) const;
		// 지정 시간의 IK 애니메이션을 평가한다.
		void EvaluateIks(float t, float animWeight) const;
		// 지정 시간의 모프 애니메이션을 평가한다.
		void EvaluateMorphs(float t, float animWeight) const;
		
	public:
		std::vector<NodeAnimationTrack> nodeTracks;
		std::vector<IkAnimationTrack> ikTracks;
		std::vector<MorphAnimationTrack> morphTracks;

		Animation(std::vector<NodeAnimationTrack> nodes, std::vector<IkAnimationTrack> iks, std::vector<MorphAnimationTrack> morphs)
			: nodeTracks(std::move(nodes)), ikTracks(std::move(iks)), morphTracks(std::move(morphs)) {}

		// 포함된 모든 트랙을 순회해 가장 마지막 키 프레임을 계산한다.
		uint32_t CalculateLastFrame() const;
		// 지정한 시간의 애니메이션 값을 모델에 평가해 적용한다.
		void Evaluate(float t, float animWeight = 1.0f) const;
	};
}
