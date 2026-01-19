#include "Node.h"

void Node::AddChild(Node* child) {
	child->m_parent = this;
	if (!m_child) {
		m_child = child;
		m_child->m_next = nullptr;
		m_child->m_prev = child;
	} else {
		const auto last = m_child->m_prev;
		last->m_next = child;
		child->m_prev = last;
		m_child->m_prev = child;
	}
}

void Node::BeginUpdateTransform() {
	m_translate = m_initTranslate;
	m_rotate = m_initRotate;
	m_scale = m_initScale;
	m_ikRotate = glm::quat(1, 0, 0, 0);
	m_appendTranslate = glm::vec3(0);
	m_appendRotate = glm::quat(1, 0, 0, 0);
}

void Node::UpdateLocalTransform() {
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

void Node::UpdateGlobalTransform() {
	m_global = m_parent ? m_parent->m_global * m_local : m_local;
	UpdateChildTransform();
}

void Node::UpdateChildTransform() const {
	Node* child = m_child;
	while (child) {
		child->UpdateGlobalTransform();
		child = child->m_next;
	}
}

void Node::UpdateAppendTransform() {
	if (m_isAppendRotate) {
		glm::quat appendRotate = !m_isAppendLocal && m_appendNode->m_appendNode
		? m_appendNode->m_appendRotate : m_appendNode->m_animRotate * m_appendNode->m_rotate;
		if (m_appendNode->m_enableIK)
			appendRotate = m_appendNode->m_ikRotate * appendRotate;
		m_appendRotate = glm::slerp(glm::quat(1, 0, 0, 0), appendRotate, m_appendWeight);
	}
	if (m_isAppendTranslate) {
		const glm::vec3 appendTranslate = !m_isAppendLocal && m_appendNode->m_appendNode
		? m_appendNode->m_appendTranslate : m_appendNode->m_animTranslate +
			m_appendNode->m_translate - m_appendNode->m_initTranslate;
		m_appendTranslate = appendTranslate * m_appendWeight;
	}
	UpdateLocalTransform();
}
