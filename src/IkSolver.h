#pragma once

#include <vector>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

struct Node;

struct IKChain
{
	Node* m_node;
	bool		m_enableAxisLimit;
	glm::vec3	m_limitMax;
	glm::vec3	m_limitMin;
	glm::vec3	m_prevAngle;
	glm::quat	m_saveIKRot;
	float		m_planeModeAngle;
};

struct IkSolver
{
	IkSolver();

	void Solve();

	std::vector<IKChain>	m_chains;
	Node*				m_ikNode;
	Node*				m_ikTarget;
	uint32_t				m_iterateCount;
	float					m_limitAngle;
	bool					m_enable;
	bool					m_baseAnimEnable;

private:
	static float NormalizeAngle(float angle);
	static float DiffAngle(float a, float b);
	static glm::vec3 Decompose(const glm::mat3& m, const glm::vec3& before);
	void SolveCore(uint32_t iteration);
	void SolvePlane(uint32_t iteration, size_t chainIdx, int RotateAxisIndex);
};
