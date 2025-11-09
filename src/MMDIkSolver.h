#pragma once

#include "MMDNode.h"

#include <vector>
#include <string>
#include <glm/vec3.hpp>

struct IKChain
{
	MMDNode* m_node;
	bool		m_enableAxisLimit;
	glm::vec3	m_limitMax;
	glm::vec3	m_limitMin;
	glm::vec3	m_prevAngle;
	glm::quat	m_saveIKRot;
	float		m_planeModeAngle;
};

enum class SolveAxis {
	X,
	Y,
	Z,
};

class MMDIkSolver
{
public:
	MMDIkSolver();

	std::string GetName() const;
	void AddIKChain(
		MMDNode* node,
		bool axisLimit,
		const glm::vec3& limixMin,
		const glm::vec3& limitMax
	);
	void Solve();
	void SaveBaseAnimation();
	void LoadBaseAnimation();
	void ClearBaseAnimation();

private:
	static float NormalizeAngle(float angle);
	static float DiffAngle(float a, float b);
	static glm::vec3 Decompose(const glm::mat3& m, const glm::vec3& before);
	void SolveCore(int iteration);
	void SolvePlane(int iteration, int chainIdx, SolveAxis solveAxis);

public:
	std::vector<IKChain>	m_chains;
	MMDNode*	m_ikNode;
	MMDNode*	m_ikTarget;
	int	m_iterateCount;
	float		m_limitAngle;
	bool		m_enable;
	bool		m_baseAnimEnable;

};

class MMDIKManager
{
public:
	int FindIKSolverIndex(const std::string& name);
	MMDIkSolver* GetIKSolver(int idx) const;
	MMDIkSolver* GetIKSolver(const std::string& ikName);
	MMDIkSolver* AddIKSolver();

	std::vector<std::unique_ptr<MMDIkSolver>>	m_ikSolvers;
};
