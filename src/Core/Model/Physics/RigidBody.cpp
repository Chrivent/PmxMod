#include "Core/Model/Physics/RigidBody.h"

#include "Core/Model/Bone/Node.h"
#include "Core/Model/ModelCoordinateConverter.h"

namespace Chrivent {
	RigidBody::RigidBody(const RigidBodyDefinition& definition, const std::shared_ptr<Node>& nodePtr) {
		switch (definition.shape) {
			case RigidBodyShape::Sphere:
				shape = std::make_unique<btSphereShape>(definition.shapeSize.x);
				break;
			case RigidBodyShape::Box:
				shape = std::make_unique<btBoxShape>(
					btVector3(definition.shapeSize.x, definition.shapeSize.y, definition.shapeSize.z));
				break;
			case RigidBodyShape::Capsule:
				shape = std::make_unique<btCapsuleShape>(definition.shapeSize.x, definition.shapeSize.y);
				break;
		}
		btScalar mass(0.0f);
		btVector3 localInertia(0, 0, 0);
		if (definition.operation != RigidBodyOperation::Static)
			mass = definition.mass;
		if (mass > 0.0f)
			shape->calculateLocalInertia(mass, localInertia);
		const auto rx = glm::rotate(glm::mat4(1), definition.rotate.x, glm::vec3(1, 0, 0));
		const auto ry = glm::rotate(glm::mat4(1), definition.rotate.y, glm::vec3(0, 1, 0));
		const auto rz = glm::rotate(glm::mat4(1), definition.rotate.z, glm::vec3(0, 0, 1));
		const glm::mat4 rotMat = ry * rx * rz;
		const glm::mat4 translateMat = glm::translate(glm::mat4(1), definition.translate);
		const glm::mat4 rbMat = ModelCoordinateConverter::ConvertZAxis(translateMat * rotMat);
		offsetMat = nodePtr ? glm::inverse(nodePtr->global) * rbMat : rbMat;
		kinematicMotionState = nodePtr
			? std::unique_ptr<MotionState>(std::make_unique<KinematicMotionState>(nodePtr, offsetMat))
			: std::unique_ptr<MotionState>(std::make_unique<DefaultMotionState>(offsetMat));
		if (definition.operation != RigidBodyOperation::Static) {
			if (nodePtr) {
				if (definition.operation == RigidBodyOperation::Dynamic)
					activeMotionState = std::make_unique<DynamicMotionState>(nodePtr, offsetMat);
				else
					activeMotionState = std::make_unique<DynamicAndBoneMergeMotionState>(nodePtr, offsetMat);
			} else
				activeMotionState = std::make_unique<DefaultMotionState>(offsetMat);
		}
		btMotionState* motionState = activeMotionState ? activeMotionState.get() : kinematicMotionState.get();
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape.get(), localInertia);
		rbInfo.m_linearDamping = definition.translateDamping;
		rbInfo.m_angularDamping = definition.rotateDamping;
		rbInfo.m_restitution = definition.restitution;
		rbInfo.m_friction = definition.friction;
		rbInfo.m_additionalDamping = true;
		rigidBody = std::make_unique<btRigidBody>(rbInfo);
		rigidBody->setUserPointer(this);
		rigidBody->setSleepingThresholds(0.01f, glm::radians(0.1f));
		rigidBody->setActivationState(DISABLE_DEACTIVATION);
		if (definition.operation == RigidBodyOperation::Static)
			rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		operation = definition.operation;
		group = definition.group;
		groupMask = definition.groupMask;
		node = nodePtr;
	}

	void RigidBody::ApplyActivation(const bool activation) const {
		if (operation != RigidBodyOperation::Static) {
			if (activation) {
				rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidBody->setMotionState(activeMotionState.get());
			} else {
				rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidBody->setMotionState(kinematicMotionState.get());
			}
		} else
			rigidBody->setMotionState(kinematicMotionState.get());
	}

	void RigidBody::ResetTransform() const {
		if (activeMotionState)
			activeMotionState->Reset();
	}

	void RigidBody::Reset() const {
		rigidBody->setAngularVelocity(btVector3(0, 0, 0));
		rigidBody->setLinearVelocity(btVector3(0, 0, 0));
		rigidBody->clearForces();
	}

	void RigidBody::ReflectGlobalTransform() const {
		if (activeMotionState)
			activeMotionState->ReflectGlobalTransform();
		kinematicMotionState->ReflectGlobalTransform();
	}

	void RigidBody::CalcLocalTransform() const {
		if (const auto nodePtr = node.lock()) {
			if (const auto parent = nodePtr->parent.lock()) {
				const auto local = glm::inverse(parent->global) * nodePtr->global;
				nodePtr->local = local;
			} else
				nodePtr->local = nodePtr->global;
		}
	}

	glm::mat4 RigidBody::CalcTransform() const {
		const btTransform transform = rigidBody->getCenterOfMassTransform();
		glm::mat4 mat;
		transform.getOpenGLMatrix(&mat[0][0]);
		return ModelCoordinateConverter::ConvertZAxis(mat);
	}
}
