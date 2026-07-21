#include "Core/Model/Physics/MotionState.h"

#include "Core/Model/Bone/Node.h"
#include "Core/Model/ModelCoordinateConverter.h"

namespace Chrivent {
	DefaultMotionState::DefaultMotionState(const glm::mat4& initialMatrix) {
		glm::mat4 trans = ModelCoordinateConverter::ConvertZAxis(initialMatrix);
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
		glm::mat4 global = ModelCoordinateConverter::ConvertZAxis(nodePtr->global * offset);
		transform.setFromOpenGLMatrix(&global[0][0]);
	}

	void DynamicMotionState::ReflectGlobalTransform() {
		const auto nodePtr = node.lock();
		if (!nodePtr)
			return;
		glm::mat4 worldTransformMat;
		transform.getOpenGLMatrix(&worldTransformMat[0][0]);
		glm::mat4 btGlobal = ModelCoordinateConverter::ConvertZAxis(worldTransformMat) * invOffset;
		PostProcessBtGlobal(btGlobal);
		nodePtr->global = btGlobal;
		nodePtr->UpdateChildTransform();
	}

	void DynamicAndBoneMergeMotionState::PostProcessBtGlobal(glm::mat4& btGlobal) const {
		if (const auto nodePtr = node.lock())
			btGlobal[3] = nodePtr->global[3];
	}

	KinematicMotionState::KinematicMotionState(const std::shared_ptr<Node>& nodePtr, const glm::mat4& offsetMatrix)
		: node(nodePtr), offset(offsetMatrix) {}

	void KinematicMotionState::getWorldTransform(btTransform& worldTransform) const {
		const auto nodePtr = node.lock();
		if (!nodePtr)
			return;
		glm::mat4 global = ModelCoordinateConverter::ConvertZAxis(nodePtr->global * offset);
		worldTransform.setFromOpenGLMatrix(&global[0][0]);
	}
}
