#pragma once

#include "MMDNode.h"

#include <vector>
#include <string>
#include <glm/vec3.hpp>

namespace saba
{
	class MMDIkSolver
	{
	public:
		MMDIkSolver();

		std::string GetName() const
		{
			if (m_ikNode != nullptr)
			{
				return m_ikNode->m_name;
			}
			else
			{
				return "";
			}
		}

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
		struct IKChain
		{
			MMDNode*	m_node;
			bool		m_enableAxisLimit;
			glm::vec3	m_limitMax;
			glm::vec3	m_limitMin;
			glm::vec3	m_prevAngle;
			glm::quat	m_saveIKRot;
			float		m_planeModeAngle;
		};

	private:
		void AddIKChain(IKChain&& chain);
		void SolveCore(uint32_t iteration);

		enum class SolveAxis {
			X,
			Y,
			Z,
		};
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
}
