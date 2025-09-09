#pragma once

#include "MMDModel.h"
#include "MMDNode.h"
#include "VMDFile.h"

#include <vector>
#include <memory>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace saba
{
	struct VMDBezier
	{
		float EvalX(float t) const;
		float EvalY(float t) const;
		glm::vec2 Eval(float t) const;
		float FindBezierX(float time) const;

		glm::vec2	m_cp1;
		glm::vec2	m_cp2;
	};

	struct VMDNodeAnimationKey
	{
		void Set(const VMDMotion& motion);

		int32_t		m_time;
		glm::vec3	m_translate;
		glm::quat	m_rotate;

		VMDBezier	m_txBezier;
		VMDBezier	m_tyBezier;
		VMDBezier	m_tzBezier;
		VMDBezier	m_rotBezier;
	};

	struct VMDMorphAnimationKey
	{
		int32_t	m_time;
		float	m_weight;
	};

	struct VMDIKAnimationKey
	{
		int32_t	m_time;
		bool	m_enable;
	};

	class VMDNodeController
	{
	public:
		VMDNodeController();

		void Evaluate(float t, float weight = 1.0f);
		void SortKeys();

		MMDNode*				m_node;
		std::vector<VMDNodeAnimationKey>	m_keys;
		size_t					m_startKeyIndex;
	};

	class VMDMorphController
	{
	public:
		VMDMorphController();

		void Evaluate(float t, float weight = 1.0f);
		void SortKeys();

		MMDMorph*				m_morph;
		std::vector<VMDMorphAnimationKey>	m_keys;
		size_t					m_startKeyIndex;
	};

	class VMDIKController
	{
	public:
		VMDIKController();

		void Evaluate(float t, float weight = 1.0f);
		void SortKeys();

		MMDIkSolver*			m_ikSolver;
		std::vector<VMDIKAnimationKey>	m_keys;
		size_t					m_startKeyIndex;
	};

	class VMDAnimation
	{
	public:
		VMDAnimation();

		bool Add(const VMDFile& vmd);
		void Destroy();
		void Evaluate(float t, float weight = 1.0f) const;

		// Physics を同期させる
		void SyncPhysics(float t, int frameCount = 30) const;

	private:
		int32_t CalculateMaxKeyTime() const;

	public:
		std::shared_ptr<MMDModel>								m_model;
		std::vector<std::unique_ptr<VMDNodeController>>			m_nodeControllers;
		std::vector<std::unique_ptr<VMDIKController>>			m_ikControllers;
		std::vector<std::unique_ptr<VMDMorphController>>		m_morphControllers;
		uint32_t												m_maxKeyTime;
	};

}
