#include "Node.h"

void Node::AddChild(const std::shared_ptr<Node>& childNode) {
	childNode->parent = shared_from_this();
	if (child.expired()) {
		child = childNode;
		childNode->next.reset();
		childNode->prev = childNode;
	} else {
		const auto head = child.lock();
		const auto last = head->prev.lock();
		last->next = childNode;
		childNode->prev = last;
		head->prev = childNode;
	}
}

void Node::BeginUpdateTransform() {
	translate = initTranslate;
	rotate = initRotate;
	scale = initScale;
	ikRotate = glm::quat(1, 0, 0, 0);
	appendTranslate = glm::vec3(0);
	appendRotate = glm::quat(1, 0, 0, 0);
}

void Node::UpdateLocalTransform() {
	glm::vec3 t = animTranslate + translate;
	if (isAppendTranslate)
		t += appendTranslate;
	glm::quat r = animRotate * rotate;
	if (enableIK)
		r = ikRotate * r;
	if (isAppendRotate)
		r = r * appendRotate;
	const glm::vec3 s = scale;
	local = glm::translate(glm::mat4(1), t)
			* glm::mat4_cast(r)
			* glm::scale(glm::mat4(1), s);
}

void Node::UpdateGlobalTransform() {
	if (const auto parentNode = parent.lock())
		global = parentNode->global * local;
	else
		global = local;
	UpdateChildTransform();
}

void Node::UpdateChildTransform() const {
	auto childNode = child.lock();
	while (childNode) {
		childNode->UpdateGlobalTransform();
		childNode = childNode->next.lock();
	}
}

void Node::UpdateAppendTransform() {
	const auto appendNodePtr = appendNode.lock();
	if (!appendNodePtr)
		return;
	if (isAppendRotate) {
		glm::quat appendRot = !isAppendLocal && !appendNodePtr->appendNode.expired()
		? appendNodePtr->appendRotate : appendNodePtr->animRotate * appendNodePtr->rotate;
		if (appendNodePtr->enableIK)
			appendRot = appendNodePtr->ikRotate * appendRot;
		appendRotate = glm::slerp(glm::quat(1, 0, 0, 0), appendRot, appendWeight);
	}
	if (isAppendTranslate) {
		const glm::vec3 appendTrans = !isAppendLocal && !appendNodePtr->appendNode.expired()
		? appendNodePtr->appendTranslate : appendNodePtr->animTranslate +
			appendNodePtr->translate - appendNodePtr->initTranslate;
		appendTranslate = appendTrans * appendWeight;
	}
	UpdateLocalTransform();
}
