#include "Node.h"

void Node::AddChild(const std::shared_ptr<Node>& child) {
	child->m_parent = shared_from_this();
	if (m_child.expired()) {
		m_child = child;
		child->m_next.reset();
		child->m_prev = child;
	} else {
		const auto head = m_child.lock();
		const auto last = head->m_prev.lock();
		last->m_next = child;
		child->m_prev = last;
		head->m_prev = child;
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
	if (const auto parent = m_parent.lock())
		m_global = parent->m_global * m_local;
	else
		m_global = m_local;
	UpdateChildTransform();
}

void Node::UpdateChildTransform() const {
	auto child = m_child.lock();
	while (child) {
		child->UpdateGlobalTransform();
		child = child->m_next.lock();
	}
}

void Node::UpdateAppendTransform() {
	const auto appendNode = m_appendNode.lock();
	if (!appendNode)
		return;
	if (m_isAppendRotate) {
		glm::quat appendRotate = !m_isAppendLocal && !appendNode->m_appendNode.expired()
		? appendNode->m_appendRotate : appendNode->m_animRotate * appendNode->m_rotate;
		if (appendNode->m_enableIK)
			appendRotate = appendNode->m_ikRotate * appendRotate;
		m_appendRotate = glm::slerp(glm::quat(1, 0, 0, 0), appendRotate, m_appendWeight);
	}
	if (m_isAppendTranslate) {
		const glm::vec3 appendTranslate = !m_isAppendLocal && !appendNode->m_appendNode.expired()
		? appendNode->m_appendTranslate : appendNode->m_animTranslate +
			appendNode->m_translate - appendNode->m_initTranslate;
		m_appendTranslate = appendTranslate * m_appendWeight;
	}
	UpdateLocalTransform();
}
