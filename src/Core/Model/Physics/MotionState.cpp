#include "MotionState.h"

#include "../Bone/Node.h"
#include "../../../Util.h"

namespace Chrivent {
	MotionState::~MotionState() = default;

	DefaultMotionState::~DefaultMotionState() = default;

	DefaultMotionState::DefaultMotionState(const glm::mat4& initialMatrix) {
		glm::mat4 trans = Util::InvZ(initialMatrix);
		transform.setFromOpenGLMatrix(&trans[0][0]);
		initialTransform = transform;
	}

	DynamicMotionState::DynamicMotionState(const std::shared_ptr<Node>& nodePtr, const glm::mat4& offsetMatrix)
		: offset(offsetMatrix), node(nodePtr) {
		invOffset = glm::inverse(offsetMatrix);
		DynamicMotionState::Reset();
	}

	void DynamicMotionState::Reset() {
		const auto nodePtr = node.lock();
		if (!nodePtr)
			return;
		glm::mat4 global = Util::InvZ(nodePtr->GetInfo().global * offset);
		transform.setFromOpenGLMatrix(&global[0][0]);
	}

	void DynamicMotionState::ReflectGlobalTransform() {
		const auto nodePtr = node.lock();
		if (!nodePtr)
			return;
		glm::mat4 worldTransformMat;
		transform.getOpenGLMatrix(&worldTransformMat[0][0]);
		glm::mat4 btGlobal = Util::InvZ(worldTransformMat) * invOffset;
		PostProcessBtGlobal(btGlobal);
		nodePtr->GetInfo().global = btGlobal;
		nodePtr->UpdateChildTransform();
	}

	void DynamicAndBoneMergeMotionState::PostProcessBtGlobal(glm::mat4& btGlobal) const {
		if (const auto nodePtr = node.lock())
			btGlobal[3] = nodePtr->GetInfo().global[3];
	}

	KinematicMotionState::KinematicMotionState(const std::shared_ptr<Node>& nodePtr, const glm::mat4& offsetMatrix)
		: node(nodePtr), offset(offsetMatrix) {}

	void KinematicMotionState::getWorldTransform(btTransform& worldTransform) const {
		const auto nodePtr = node.lock();
		if (!nodePtr)
			return;
		glm::mat4 global = Util::InvZ(nodePtr->GetInfo().global * offset);
		worldTransform.setFromOpenGLMatrix(&global[0][0]);
	}
}
