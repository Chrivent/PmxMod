#pragma once

#include <memory>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	struct Node;

	struct IkChain {
		std::weak_ptr<Node>	node;
		bool				enableAxisLimit;
		glm::vec3			limitMax;
		glm::vec3			limitMin;
		glm::vec3			prevAngle;
		glm::quat			saveIkRot;
		float				planeModeAngle;
	};

	struct IkSolver {
		std::vector<IkChain>	chains;
		std::weak_ptr<Node>		ikNode;
		std::weak_ptr<Node>		ikTarget;
		uint32_t				iterateCount = 1;
		float					limitAngle = glm::two_pi<float>();
		bool					enable = true;
		bool					baseAnimEnable = true;

		// IK 체인을 반복 계산해 타깃 노드에 맞춘다.
		void Solve();
		// 단일 반복 단계에서 일반 IK 체인을 계산한다.
		void SolveCore(uint32_t iteration);
		// 축 제한이 평면 모드인 체인 요소를 계산한다.
		void SolvePlane(uint32_t iteration, size_t chainIdx, int rotateAxisIndex);
		
	private:
		// 각도를 [0, 2pi) 범위로 정규화한다.
		static float NormalizeAngle(float angle);
		// 두 각도의 최단 차이를 [-pi, pi] 범위로 계산한다.
		static float DiffAngle(float a, float b);
		// 회전 행렬을 이전 Euler 각도와 가장 가까운 XYZ Euler 각도로 분해한다.
		static glm::vec3 Decompose(const glm::mat3& m, const glm::vec3& before);
	};
}
