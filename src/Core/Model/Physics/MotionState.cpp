#include "Core/Model/Physics/MotionState.h"

#include "Core/Model/Bone/Node.h"
#include "Core/Model/ModelCoordinateConverter.h"

namespace Chrivent {
	DefaultMotionState::DefaultMotionState(const glm::mat4& initialMatrix) {
		glm::mat4 trans = ModelCoordinateConverter::ConvertZAxis(initialMatrix);
		transform.setFromOpenGLMatrix(&trans[0][0]);
		initialTransform = transform;
	}

	DynamicMotionState::DynamicMotionState(Node* sourceNode, const glm::mat4& offsetMatrix)
		: offset(offsetMatrix), node(sourceNode) {
		invOffset = glm::inverse(offsetMatrix);
		DynamicMotionState::Reset();
	}

	void DynamicMotionState::Reset() {
		if (!node)
			return;
		glm::mat4 global = ModelCoordinateConverter::ConvertZAxis(node->global * offset);
		transform.setFromOpenGLMatrix(&global[0][0]);
	}

	void DynamicMotionState::ReflectGlobalTransform() {
		if (!node)
			return;
		glm::mat4 worldTransformMat;
		transform.getOpenGLMatrix(&worldTransformMat[0][0]);
		glm::mat4 btGlobal = ModelCoordinateConverter::ConvertZAxis(worldTransformMat) * invOffset;
		PostProcessBtGlobal(btGlobal);
		node->global = btGlobal;
		node->UpdateChildTransform();
	}

	void DynamicAndBoneMergeMotionState::PostProcessBtGlobal(glm::mat4& btGlobal) const {
		if (node)
			btGlobal[3] = node->global[3];
	}

	KinematicMotionState::KinematicMotionState(Node* sourceNode, const glm::mat4& offsetMatrix)
		: node(sourceNode), offset(offsetMatrix) {}

	void KinematicMotionState::getWorldTransform(btTransform& worldTransform) const {
		if (!node)
			return;
		glm::mat4 global = ModelCoordinateConverter::ConvertZAxis(node->global * offset);
		worldTransform.setFromOpenGLMatrix(&global[0][0]);
	}
}
