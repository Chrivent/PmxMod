#include "Core/Model/Physics/MotionState.h"

#include "Core/Model/Bone/Node.h"
#include "Core/Model/ModelCoordinateConverter.h"

namespace Chrivent {
	btTransform MotionState::ToWorldTransform(const glm::mat4& modelMatrix) const {
		const glm::mat4 matrix = ModelCoordinateConverter::ConvertZAxis(modelMatrix);
		btTransform result;
		result.setFromOpenGLMatrix(&matrix[0][0]);
		result.setOrigin(result.getOrigin() * sceneScale);
		return result;
	}

	DefaultMotionState::DefaultMotionState(const glm::mat4& initialMatrix, const float scale)
		: MotionState(scale), initialTransform(ToWorldTransform(initialMatrix)), transform(initialTransform) {}

	DynamicMotionState::DynamicMotionState(Node* sourceNode, const glm::mat4& offsetMatrix, const float scale)
		: MotionState(scale), offset(offsetMatrix), node(sourceNode) {
		invOffset = glm::inverse(offsetMatrix);
		DynamicMotionState::Reset();
	}

	void DynamicMotionState::Reset() {
		if (!node)
			return;
		transform = ToWorldTransform(node->global * offset);
	}

	void DynamicMotionState::ReflectGlobalTransform() {
		if (!node)
			return;
		glm::mat4 worldTransformMat;
		transform.getOpenGLMatrix(&worldTransformMat[0][0]);
		worldTransformMat[3] = glm::vec4(glm::vec3(worldTransformMat[3]) / sceneScale, 1.0f);
		glm::mat4 btGlobal = ModelCoordinateConverter::ConvertZAxis(worldTransformMat) * invOffset;
		PostProcessBtGlobal(btGlobal);
		node->global = btGlobal;
		node->local = node->parent ? glm::inverse(node->parent->global) * btGlobal : btGlobal;
		node->UpdateChildTransform();
	}

	void DynamicAndBoneMergeMotionState::PostProcessBtGlobal(glm::mat4& btGlobal) const {
		if (node)
			btGlobal[3] = node->global[3];
	}

	KinematicMotionState::KinematicMotionState(Node* sourceNode, const glm::mat4& offsetMatrix, const float scale)
		: MotionState(scale), node(sourceNode), offset(offsetMatrix) {}

	void KinematicMotionState::getWorldTransform(btTransform& worldTransform) const {
		if (!node)
			return;
		worldTransform = ToWorldTransform(node->global * offset);
	}
}
