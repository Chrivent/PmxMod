#pragma once

#include "../Bezier.h"

#include <memory>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	struct Morph;
	struct Model;
	struct Node;
	struct IkSolver;

	struct NodeAnimationKey {
		int32_t		time;
		glm::vec3	translate;
		glm::quat	rotate;
		Bezier		txBezier;
		Bezier		tyBezier;
		Bezier		tzBezier;
		Bezier		rotBezier;
	};

	struct MorphAnimationKey {
		int32_t	time;
		float	morphWeight;
	};

	struct IkAnimationKey {
		int32_t	time;
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

	struct AnimationInfo {
		std::shared_ptr<Model> model;
		std::vector<NodeAnimationTrack> nodeTracks;
		std::vector<IkAnimationTrack> ikTracks;
		std::vector<MorphAnimationTrack> morphTracks;
	};

	class Animation {
		AnimationInfo info;

	public:
		const AnimationInfo& GetInfo() const { return info; }
		void SetInfo(AnimationInfo animationInfo) { info = std::move(animationInfo); }
		
		// 애니메이션 트랙과 연결 상태를 해제한다.
		void Destroy();
		// 포함된 모든 트랙 중 가장 마지막 키 프레임을 반환한다.
		int32_t GetLastFrame() const;
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
