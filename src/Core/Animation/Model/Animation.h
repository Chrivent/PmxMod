#pragma once

#include "../Bezier.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	struct Morph;
	class Node;
	class IkSolver;

	struct NodeAnimationKey {
		uint32_t	frame = 0;
		glm::vec3	translate;
		glm::quat	rotate;
		Bezier		txBezier;
		Bezier		tyBezier;
		Bezier		tzBezier;
		Bezier		rotBezier;
	};

	struct MorphAnimationKey {
		uint32_t	frame = 0;
		float	morphWeight;
	};

	struct IkAnimationKey {
		uint32_t	frame = 0;
		bool	ikEnable;
	};

	struct NodeAnimationTrack {
		std::shared_ptr<Node> node;
		std::vector<NodeAnimationKey> keys;
	};

	struct IkAnimationTrack {
		std::shared_ptr<IkSolver> ikSolver;
		std::vector<IkAnimationKey> keys;
	};

	struct MorphAnimationTrack {
		std::shared_ptr<Morph> morph;
		std::vector<MorphAnimationKey> keys;
	};

	class Animation {
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
		
	private:
		// 지정 시간의 노드 애니메이션을 평가한다.
		void EvaluateNodes(float t, float animWeight) const;
		// 지정 시간의 IK 애니메이션을 평가한다.
		void EvaluateIks(float t, float animWeight) const;
		// 지정 시간의 모프 애니메이션을 평가한다.
		void EvaluateMorphs(float t, float animWeight) const;
	};
}
