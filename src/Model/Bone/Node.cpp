#include "Node.h"

namespace Chrivent {
	void Node::AddChild(const std::shared_ptr<Node>& childNode) {
		childNode->info.parent = shared_from_this();
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
		info.translate = info.initTranslate;
		info.rotate = info.initRotate;
		info.scale = info.initScale;
		info.ikRotate = glm::quat(1, 0, 0, 0);
		appendTranslate = glm::vec3(0);
		appendRotate = glm::quat(1, 0, 0, 0);
	}

	void Node::UpdateLocalTransform() {
		glm::vec3 t = info.animTranslate + info.translate;
		if (info.isAppendTranslate)
			t += appendTranslate;
		glm::quat r = info.animRotate * info.rotate;
		if (info.enableIk)
			r = info.ikRotate * r;
		if (info.isAppendRotate)
			r = r * appendRotate;
		const glm::vec3 s = info.scale;
		info.local = glm::translate(glm::mat4(1), t)
				* glm::mat4_cast(r)
				* glm::scale(glm::mat4(1), s);
	}

	void Node::UpdateGlobalTransform() {
		if (const auto parentNode = info.parent.lock())
			info.global = parentNode->info.global * info.local;
		else
			info.global = info.local;
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
		const auto appendNodePtr = info.appendNode.lock();
		if (!appendNodePtr)
			return;
		if (info.isAppendRotate) {
			glm::quat appendRot = !info.isAppendLocal && !appendNodePtr->info.appendNode.expired()
			? appendNodePtr->appendRotate : appendNodePtr->info.animRotate * appendNodePtr->info.rotate;
			if (appendNodePtr->info.enableIk)
				appendRot = appendNodePtr->info.ikRotate * appendRot;
			appendRotate = glm::slerp(glm::quat(1, 0, 0, 0), appendRot, info.appendWeight);
		}
		if (info.isAppendTranslate) {
			const glm::vec3 appendTrans = !info.isAppendLocal && !appendNodePtr->info.appendNode.expired()
			? appendNodePtr->appendTranslate : appendNodePtr->info.animTranslate +
				appendNodePtr->info.translate - appendNodePtr->info.initTranslate;
			appendTranslate = appendTrans * info.appendWeight;
		}
		UpdateLocalTransform();
	}
}
