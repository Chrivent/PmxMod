#include "Core/Model/Bone/Node.h"

namespace Chrivent {
	void Node::AddChild(Node& childNode) {
		childNode.parent = this;
		childNode.next = nullptr;
		if (!child)
			child = &childNode;
		else
			lastChild->next = &childNode;
		lastChild = &childNode;
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
		if (enableIk)
			r = ikRotate * r;
		if (isAppendRotate)
			r = r * appendRotate;
		const glm::vec3 s = scale;
		local = glm::translate(glm::mat4(1), t)
				* glm::mat4_cast(r)
				* glm::scale(glm::mat4(1), s);
	}

	void Node::UpdateGlobalTransform() {
		if (parent)
			global = parent->global * local;
		else
			global = local;
		UpdateChildTransform();
	}

	void Node::UpdateChildTransform() const {
		Node* node = child;
		while (node) {
			if (node->parent)
				node->global = node->parent->global * node->local;
			else
				node->global = node->local;
			if (node->child) {
				node = node->child;
				continue;
			}
			while (node) {
				if (node->next) {
					node = node->next;
					break;
				}
				Node* parentNode = node->parent;
				if (!parentNode || parentNode == this) {
					node = nullptr;
					break;
				}
				node = parentNode;
			}
		}
	}

	void Node::UpdateAppendTransform() {
		if (!appendNode)
			return;
		if (isAppendRotate) {
			glm::quat appendRot;
			if (isAppendLocal) {
				// 부여와 IK, 물리가 끝난 로컬 행렬에서 스케일을 제외한 회전만 받는다.
				glm::mat3 basis(appendNode->local);
				for (int axis = 0; axis < 3; ++axis) {
					const float length = glm::length(basis[axis]);
					if (length > 0.0f)
						basis[axis] /= length;
				}
				appendRot = glm::normalize(glm::quat_cast(basis));
			} else {
				appendRot = appendNode->appendNode ? appendNode->appendRotate : appendNode->animRotate * appendNode->rotate;
				if (appendNode->enableIk)
					appendRot = appendNode->ikRotate * appendRot;
			}
			appendRotate = glm::slerp(glm::quat(1, 0, 0, 0), appendRot, appendWeight);
		}
		if (isAppendTranslate) {
			const glm::vec3 appendTrans = isAppendLocal ? glm::vec3(appendNode->local[3]) - appendNode->initTranslate
				: appendNode->appendNode ? appendNode->appendTranslate : appendNode->animTranslate +
					appendNode->translate - appendNode->initTranslate;
			appendTranslate = appendTrans * appendWeight;
		}
		UpdateLocalTransform();
	}
}
