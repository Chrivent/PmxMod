#include "Core/Model/Physics/RigidBody.h"

#include "Core/Model/Bone/Node.h"
#include "Core/Model/ModelCoordinateConverter.h"

namespace Chrivent {
	RigidBody::RigidBody(const RigidBodyDefinition& definition, Node* sourceNode, const float scale) {
		const glm::vec3 size = definition.shapeSize * std::abs(scale);
		switch (definition.shape) {
			case RigidBodyShape::Sphere:
				shape = std::make_unique<btSphereShape>(size.x);
				break;
			case RigidBodyShape::Box:
				shape = std::make_unique<btBoxShape>(
					btVector3(size.x, size.y, size.z));
				break;
			case RigidBodyShape::Capsule:
				shape = std::make_unique<btCapsuleShape>(size.x, size.y);
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
		const glm::mat4 offsetMat = sourceNode ? glm::inverse(sourceNode->global) * rbMat : rbMat;
		if (!sourceNode)
			motionState = std::make_unique<DefaultMotionState>(offsetMat, scale);
		else if (definition.operation == RigidBodyOperation::Static)
			motionState = std::make_unique<KinematicMotionState>(sourceNode, offsetMat, scale);
		else {
			for (const Node* ancestor = sourceNode; ancestor; ancestor = ancestor->parent)
				++poseDepth;
			if (definition.operation == RigidBodyOperation::Dynamic)
				motionState = std::make_unique<DynamicMotionState>(sourceNode, offsetMat, scale);
			else
				motionState = std::make_unique<DynamicAndBoneMergeMotionState>(sourceNode, offsetMat, scale);
		}
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), shape.get(), localInertia);
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
	}

	void RigidBody::ResetTransform() const {
		motionState->Reset();
		btTransform transform;
		motionState->getWorldTransform(transform);
		rigidBody->setCenterOfMassTransform(transform);
		rigidBody->setInterpolationWorldTransform(transform);
		rigidBody->setInterpolationLinearVelocity(btVector3(0, 0, 0));
		rigidBody->setInterpolationAngularVelocity(btVector3(0, 0, 0));
		rigidBody->setAngularVelocity(btVector3(0, 0, 0));
		rigidBody->setLinearVelocity(btVector3(0, 0, 0));
		rigidBody->clearForces();
		rigidBody->activate(true);
	}

}
