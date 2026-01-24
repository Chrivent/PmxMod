#pragma once

#include <vector>
#include <glm/gtc/quaternion.hpp>

struct Node;

struct IKChain {
	Node*		m_node;
	bool		m_enableAxisLimit;
	glm::vec3	m_limitMax;
	glm::vec3	m_limitMin;
	glm::vec3	m_prevAngle;
	glm::quat	m_saveIKRot;
	float		m_planeModeAngle;
};

struct IkSolver {
	std::vector<IKChain>	m_chains;
	Node*					m_ikNode = nullptr;
	Node*					m_ikTarget = nullptr;
	uint32_t				m_iterateCount = 1;
	float					m_limitAngle = glm::two_pi<float>();
	bool					m_enable = true;
	bool					m_baseAnimEnable = true;

	void Solve();

private:
	void SolveCore(uint32_t iteration);
	void SolvePlane(uint32_t iteration, size_t chainIdx, int RotateAxisIndex);
};
