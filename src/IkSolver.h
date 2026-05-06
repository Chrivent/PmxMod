#pragma once

#include <memory>
#include <vector>
#include <glm/gtc/quaternion.hpp>

struct Node;

struct IKChain {
	std::weak_ptr<Node>	m_node;
	bool				m_enableAxisLimit;
	glm::vec3			m_limitMax;
	glm::vec3			m_limitMin;
	glm::vec3			m_prevAngle;
	glm::quat			m_saveIKRot;
	float				m_planeModeAngle;
};

struct IkSolver {
	std::vector<IKChain>	m_chains;
	std::weak_ptr<Node>		m_ikNode;
	std::weak_ptr<Node>		m_ikTarget;
	uint32_t				m_iterateCount = 1;
	float					m_limitAngle = glm::two_pi<float>();
	bool					m_enable = true;
	bool					m_baseAnimEnable = true;

	/// IK 체인을 반복 계산해 타깃 노드에 맞춘다.
	void Solve();
	/// 단일 반복 단계에서 일반 IK 체인을 계산한다.
	void SolveCore(uint32_t iteration);
	/// 축 제한이 평면 모드인 체인 요소를 계산한다.
	void SolvePlane(uint32_t iteration, size_t chainIdx, int RotateAxisIndex);
};
