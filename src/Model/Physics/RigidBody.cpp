#include "RigidBody.h"

#include "Physics.h"
#include "../Bone/Node.h"
#include "../Model.h"
#include "../../Util.h"

namespace Chrivent {
	void RigidBody::Create(const PmxParser::PmxRigidbody& pmxRigidBody, const Model* model, const std::shared_ptr<Node>& nodePtr) {
		switch (pmxRigidBody.shape) {
			case Shape::Sphere:
				shape = std::make_unique<btSphereShape>(pmxRigidBody.shapeSize.x);
				break;
			case Shape::Box:
				shape = std::make_unique<btBoxShape>(btVector3(
					pmxRigidBody.shapeSize.x,
					pmxRigidBody.shapeSize.y,
					pmxRigidBody.shapeSize.z
				));
				break;
			case Shape::Capsule:
				shape = std::make_unique<btCapsuleShape>(
					pmxRigidBody.shapeSize.x,
					pmxRigidBody.shapeSize.y
				);
				break;
		}
		btScalar mass(0.0f);
		btVector3 localInertia(0, 0, 0);
		if (pmxRigidBody.op != Operation::Static)
			mass = pmxRigidBody.mass;
		if (mass > 0.0f)
			shape->calculateLocalInertia(mass, localInertia);
		const auto rx = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.x, glm::vec3(1, 0, 0));
		const auto ry = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.y, glm::vec3(0, 1, 0));
		const auto rz = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.z, glm::vec3(0, 0, 1));
		const glm::mat4 rotMat = ry * rx * rz;
		const glm::mat4 translateMat = glm::translate(glm::mat4(1), pmxRigidBody.translate);
		const glm::mat4 rbMat = Util::InvZ(translateMat * rotMat);
		offsetMat = nodePtr ? glm::inverse(nodePtr->global) * rbMat : rbMat;
		kinematicMotionState = nodePtr
			? std::unique_ptr<MotionState>(std::make_unique<KinematicMotionState>(nodePtr, offsetMat))
			: std::unique_ptr<MotionState>(std::make_unique<DefaultMotionState>(offsetMat));
		if (pmxRigidBody.op != Operation::Static) {
			if (nodePtr) {
				if (pmxRigidBody.op == Operation::Dynamic)
					activeMotionState = std::make_unique<DynamicMotionState>(nodePtr, offsetMat);
				else
					activeMotionState = std::make_unique<DynamicAndBoneMergeMotionState>(nodePtr, offsetMat);
			} else
				activeMotionState = std::make_unique<DefaultMotionState>(offsetMat);
		}
		btMotionState* motionState = activeMotionState ? activeMotionState.get() : kinematicMotionState.get();
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape.get(), localInertia);
		rbInfo.m_linearDamping = pmxRigidBody.translateDimmer;
		rbInfo.m_angularDamping = pmxRigidBody.rotateDimmer;
		rbInfo.m_restitution = pmxRigidBody.repulsion;
		rbInfo.m_friction = pmxRigidBody.friction;
		rbInfo.m_additionalDamping = true;
		rigidBody = std::make_unique<btRigidBody>(rbInfo);
		rigidBody->setUserPointer(this);
		rigidBody->setSleepingThresholds(0.01f, glm::radians(0.1f));
		rigidBody->setActivationState(DISABLE_DEACTIVATION);
		if (pmxRigidBody.op == Operation::Static)
			rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
		rigidBodyType = pmxRigidBody.op;
		group = pmxRigidBody.group;
		groupMask = pmxRigidBody.collisionGroup;
		node = nodePtr;
		name = pmxRigidBody.name;
	}

	void RigidBody::ApplyActivation(const bool activation) const {
		if (rigidBodyType != Operation::Static) {
			if (activation) {
				rigidBody->setCollisionFlags(
					rigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidBody->setMotionState(activeMotionState.get());
			} else {
				rigidBody->setCollisionFlags(
					rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidBody->setMotionState(kinematicMotionState.get());
			}
		} else
			rigidBody->setMotionState(kinematicMotionState.get());
	}

	void RigidBody::ResetTransform() const {
		if (activeMotionState)
			activeMotionState->Reset();
	}

	void RigidBody::Reset(const Physics* physics) const {
		if (const auto cache = physics->world->getPairCache()) {
			const auto dispatcher = physics->world->getDispatcher();
			cache->cleanProxyFromPairs(rigidBody->getBroadphaseHandle(), dispatcher);
		}
		rigidBody->setAngularVelocity(btVector3(0, 0, 0));
		rigidBody->setLinearVelocity(btVector3(0, 0, 0));
		rigidBody->clearForces();
	}

	void RigidBody::ReflectGlobalTransform() const {
		if (activeMotionState)
			activeMotionState->ReflectGlobalTransform();
		if (kinematicMotionState)
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
		return Util::InvZ(mat);
	}
}
