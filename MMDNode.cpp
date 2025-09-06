#include "MMDNode.h"

#include <glm/gtc/matrix_transform.hpp>

namespace saba
{
	MMDNode::MMDNode()
		: m_index(0)
		, m_enableIK(false)
		, m_parent(nullptr)
		, m_child(nullptr)
		, m_next(nullptr)
		, m_prev(nullptr)
		, m_translate(0)
		, m_rotate(1, 0, 0, 0)
		, m_scale(1)
		, m_animTranslate(0)
		, m_animRotate(1, 0, 0, 0)
		, m_baseAnimTranslate(0)
		, m_baseAnimRotate(1, 0, 0, 0)
		, m_ikRotate(1, 0, 0, 0)
		, m_local(1)
		, m_global(1)
		, m_inverseInit(1)
		, m_initTranslate(0)
		, m_initRotate(1, 0, 0, 0)
		, m_initScale(1)
	{
	}

	void MMDNode::AddChild(MMDNode * child)
	{
		if (child == nullptr)
		{
			return;
		}

		child->m_parent = this;
		if (m_child == nullptr)
		{
			m_child = child;
			m_child->m_next = nullptr;
			m_child->m_prev = m_child;
		}
		else
		{
			auto lastNode = m_child->m_prev;
			lastNode->m_next = child;
			child->m_prev = lastNode;

			m_child->m_prev = child;
		}
	}

	void MMDNode::BeginUpdateTransform()
	{
		LoadInitialTRS();
		m_ikRotate = glm::quat(1, 0, 0, 0);
		OnBeginUpdateTransform();
	}

	void MMDNode::EndUpdateTransform()
	{
		OnEndUpdateTransfrom();
	}

	void MMDNode::UpdateLocalTransform()
	{
		OnUpdateLocalTransform();
	}

	void MMDNode::UpdateGlobalTransform()
	{
		if (m_parent == nullptr)
		{
			m_global = m_local;
		}
		else
		{
			m_global = m_parent->m_global * m_local;
		}
		MMDNode* child = m_child;
		while (child != nullptr)
		{
			child->UpdateGlobalTransform();
			child = child->m_next;
		}
	}

	void MMDNode::UpdateChildTransform()
	{
		MMDNode* child = m_child;
		while (child != nullptr)
		{
			child->UpdateGlobalTransform();
			child = child->m_next;
		}
	}

	glm::vec3 MMDNode::AnimateTranslate() const
	{
		return m_animTranslate + m_translate;
	}

	glm::quat MMDNode::AnimateRotate() const
	{
		return m_animRotate * m_rotate;
	}

	void MMDNode::CalculateInverseInitTransform()
	{
		m_inverseInit = glm::inverse(m_global);
	}

	void MMDNode::OnBeginUpdateTransform()
	{
	}

	void MMDNode::OnEndUpdateTransfrom()
	{
	}

	void MMDNode::OnUpdateLocalTransform()
	{
		auto s = glm::scale(glm::mat4(1), m_scale);
		auto r = glm::mat4_cast(AnimateRotate());
		auto t = glm::translate(glm::mat4(1), AnimateTranslate());
		if (m_enableIK)
		{
			r = glm::mat4_cast(m_ikRotate) * r;
		}
		m_local = t * r * s;
	}

}
