#pragma once

#include "Core/Animation/Bezier.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	struct Morph;
	class Model;
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
		Node* node = nullptr;
		std::vector<NodeAnimationKey> keys;
	};

	// IK 대상 하나에 적용할 활성 키 목록을 보관한다.
	struct IkAnimationTrack {
		IkSolver* ikSolver = nullptr;
		std::vector<IkAnimationKey> keys;
	};

	// 모프 하나에 적용할 가중치 키 목록을 보관한다.
	struct MorphAnimationTrack {
		Morph* morph = nullptr;
		std::vector<MorphAnimationKey> keys;
	};

	// 모델의 본, 모프와 IK 트랙을 시간에 따라 평가한다.
	class Animation {
		// 트랙의 비소유 대상 포인터가 유효하도록 대상 모델의 수명을 유지한다.
		std::shared_ptr<Model> targetModel;
		std::vector<NodeAnimationTrack> nodeTracks;
		std::vector<IkAnimationTrack> ikTracks;
		std::vector<MorphAnimationTrack> morphTracks;

		// 지정 시간의 노드 애니메이션을 평가한다.
		void EvaluateNodes(float t, float animWeight) const;
		// 지정 시간의 IK 애니메이션을 평가한다.
		void EvaluateIks(float t, float animWeight) const;
		// 지정 시간의 모프 애니메이션을 평가한다.
		void EvaluateMorphs(float t, float animWeight) const;
		
	public:
		Animation(std::shared_ptr<Model> targetModel, std::vector<NodeAnimationTrack> nodes,
			std::vector<IkAnimationTrack> iks, std::vector<MorphAnimationTrack> morphs)
			: targetModel(std::move(targetModel)), nodeTracks(std::move(nodes)),
			ikTracks(std::move(iks)), morphTracks(std::move(morphs)) {}

		const std::vector<NodeAnimationTrack>& GetNodeTracks() const { return nodeTracks; }
		const std::vector<IkAnimationTrack>& GetIkTracks() const { return ikTracks; }
		const std::vector<MorphAnimationTrack>& GetMorphTracks() const { return morphTracks; }

		// 포함된 모든 트랙을 순회해 가장 마지막 키 프레임을 계산한다.
		uint32_t CalculateLastFrame() const;
		// 지정한 시간의 애니메이션 값을 모델에 평가해 적용한다.
		void Evaluate(float t, float animWeight = 1.0f) const;
	};
}
