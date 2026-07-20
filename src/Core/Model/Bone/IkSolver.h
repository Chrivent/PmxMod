#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace Chrivent {
	class Node;

	// IK 체인 본과 각도 제한 상태를 보관한다.
	struct IkChain {
		std::weak_ptr<Node>	node;
		bool				enableAxisLimit;
		glm::vec3			limitMax;
		glm::vec3			limitMin;
		glm::vec3			prevAngle;
		glm::quat			saveIkRot;
		float				planeModeAngle;
	};

	// 반복 역기구학으로 목표 본을 향하도록 IK 체인을 계산한다.
	class IkSolver {
		// 길이가 충분한 벡터만 단위 방향으로 변환한다.
		static bool NormalizeDirection(const glm::vec3& value, glm::vec3& direction);
		// 두 방향의 외적이 퇴화한 경우에도 사용할 수 있는 회전축을 계산한다.
		static bool ResolveRotationAxis(const glm::vec3& from, const glm::vec3& to, glm::vec3& axis);

		// 단일 반복 단계에서 일반 IK 체인을 계산한다.
		void SolveCore(uint32_t iteration);
		// 축 제한이 평면 모드인 체인 요소를 계산한다.
		void SolvePlane(uint32_t iteration, size_t chainIdx, int rotateAxisIndex);
		// 각도를 [0, 2pi) 범위로 정규화한다.
		static float NormalizeAngle(float angle);
		// 두 각도의 최단 차이를 [-pi, pi] 범위로 계산한다.
		static float DiffAngle(float a, float b);
		// 회전 행렬을 이전 Euler 각도와 가장 가까운 XYZ Euler 각도로 분해한다.
		static glm::vec3 Decompose(const glm::mat3& m, const glm::vec3& before);

	public:
		std::vector<IkChain>	chains;
		std::weak_ptr<Node>		ikNode;
		std::weak_ptr<Node>		ikTarget;
		uint32_t				iterateCount = 1;
		float					limitAngle = glm::two_pi<float>();
		bool					enable = true;
		bool					baseAnimEnable = true;
		
		// IK 체인을 반복 계산해 타깃 노드에 맞춘다.
		void Solve();
	};
}
