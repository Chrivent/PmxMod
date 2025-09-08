#pragma once

#include "MMDNode.h"

#include <vector>
#include <string>
#include <glm/vec3.hpp>

namespace saba
{
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

		void AddIKChain(MMDNode* node, bool isKnee = false);
		void AddIKChain(
			MMDNode* node,
			bool axisLimit,
			const glm::vec3& limixMin,
			const glm::vec3& limitMax
		);

		void Solve();

		void SaveBaseAnimation() { m_baseAnimEnable = m_enable; }
		void LoadBaseAnimation() { m_enable = m_baseAnimEnable; }
		void ClearBaseAnimation() { m_baseAnimEnable = true; }

	private:
		void AddIKChain(IKChain&& chain);
		void SolveCore(uint32_t iteration);

		void SolvePlane(uint32_t iteration, size_t chainIdx, SolveAxis solveAxis);

	public:
		std::vector<IKChain>	m_chains;
		MMDNode*	m_ikNode;
		MMDNode*	m_ikTarget;
		uint32_t	m_iterateCount;
		float		m_limitAngle;
		bool		m_enable;
		bool		m_baseAnimEnable;

	};

	class MMDIKManager
	{
	public:
		static const size_t NPos = -1;

		size_t FindIKSolverIndex(const std::string& name);
		MMDIkSolver* GetIKSolver(size_t idx);
		MMDIkSolver* GetIKSolver(const std::string& ikName);
		MMDIkSolver* AddIKSolver();

		std::vector<std::unique_ptr<MMDIkSolver>>	m_ikSolvers;
	};
}
