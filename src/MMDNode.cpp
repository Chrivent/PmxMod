#include "MMDNode.h"

#include <glm/gtc/matrix_transform.hpp>

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
	, m_deformDepth(-1)
	, m_isDeformAfterPhysics(false)
	, m_appendNode(nullptr)
	, m_isAppendRotate(false)
	, m_isAppendTranslate(false)
	, m_isAppendLocal(false)
	, m_appendWeight(0)
	, m_ikSolver(nullptr) {
}

void MMDNode::AddChild(MMDNode* child) {
	child->m_parent = this;
	if (m_child == nullptr) {
		m_child = child;
		m_child->m_next = nullptr;
		m_child->m_prev = m_child;
	} else {
		const auto lastNode = m_child->m_prev;
		lastNode->m_next = child;
		child->m_prev = lastNode;
		m_child->m_prev = child;
	}
}

void MMDNode::BeginUpdateTransform() {
	m_translate = m_initTranslate;
	m_rotate = m_initRotate;
	m_scale = m_initScale;
	m_ikRotate = glm::quat(1, 0, 0, 0);
	m_appendTranslate = glm::vec3(0);
	m_appendRotate = glm::quat(1, 0, 0, 0);
}

void MMDNode::UpdateLocalTransform() {
	glm::vec3 t = m_animTranslate + m_translate;
	if (m_isAppendTranslate)
		t += m_appendTranslate;
	glm::quat r = m_animRotate * m_rotate;
	if (m_enableIK)
		r = m_ikRotate * r;
	if (m_isAppendRotate)
		r = r * m_appendRotate;
	const glm::vec3 s = m_scale;
	m_local = glm::translate(glm::mat4(1), t)
			* glm::mat4_cast(r)
			* glm::scale(glm::mat4(1), s);
}

void MMDNode::UpdateGlobalTransform() {
	if (m_parent == nullptr)
		m_global = m_local;
	else
		m_global = m_parent->m_global * m_local;
	MMDNode *child = m_child;
	while (child != nullptr) {
		child->UpdateGlobalTransform();
		child = child->m_next;
	}
}

void MMDNode::UpdateChildTransform() const {
	MMDNode *child = m_child;
	while (child != nullptr) {
		child->UpdateGlobalTransform();
		child = child->m_next;
	}
}

void MMDNode::UpdateAppendTransform() {
	if (m_isAppendRotate) {
		glm::quat appendRotate;
		if (!m_isAppendLocal && m_appendNode->m_appendNode != nullptr)
			appendRotate = m_appendNode->m_appendRotate;
		else
			appendRotate = m_appendNode->m_animRotate * m_appendNode->m_rotate;
		if (m_appendNode->m_enableIK)
			appendRotate = m_appendNode->m_ikRotate * appendRotate;
		const glm::quat appendQ = glm::slerp(
			glm::quat(1, 0, 0, 0),
			appendRotate,
			m_appendWeight
		);
		m_appendRotate = appendQ;
	}
	if (m_isAppendTranslate) {
		glm::vec3 appendTranslate;
		if (!m_isAppendLocal && m_appendNode->m_appendNode != nullptr)
			appendTranslate = m_appendNode->m_appendTranslate;
		else
			appendTranslate = m_appendNode->m_translate - m_appendNode->m_initTranslate;
		m_appendTranslate = appendTranslate * m_appendWeight;
	}
	UpdateLocalTransform();
}
