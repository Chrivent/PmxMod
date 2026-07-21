#include "Core/Model/Physics/Joint.h"

#include "Core/Model/Physics/RigidBody.h"

namespace Chrivent {
	Joint::Joint(const JointDefinition& definition, const RigidBody& rigidBodyA, const RigidBody& rigidBodyB) {
		btMatrix3x3 rotMat;
		rotMat.setEulerZYX(definition.rotate.x, definition.rotate.y, definition.rotate.z);
		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(btVector3(definition.translate.x, definition.translate.y, definition.translate.z));
		transform.setBasis(rotMat);
		btTransform invA = rigidBodyA.GetRigidBody().getWorldTransform().inverse();
		btTransform invB = rigidBodyB.GetRigidBody().getWorldTransform().inverse();
		invA = invA * transform;
		invB = invB * transform;
		auto jointConstraint = std::make_unique<btGeneric6DofSpringConstraint>(
			rigidBodyA.GetRigidBody(), rigidBodyB.GetRigidBody(),
			invA, invB, true);
		jointConstraint->setLinearLowerLimit(btVector3(definition.translateLowerLimit.x, definition.translateLowerLimit.y, definition.translateLowerLimit.z));
		jointConstraint->setLinearUpperLimit(btVector3(definition.translateUpperLimit.x, definition.translateUpperLimit.y, definition.translateUpperLimit.z));
		jointConstraint->setAngularLowerLimit(btVector3(definition.rotateLowerLimit.x, definition.rotateLowerLimit.y, definition.rotateLowerLimit.z));
		jointConstraint->setAngularUpperLimit(btVector3(definition.rotateUpperLimit.x, definition.rotateUpperLimit.y, definition.rotateUpperLimit.z));
		const float stiffness[6] = {
			definition.springTranslateFactor.x,
			definition.springTranslateFactor.y,
			definition.springTranslateFactor.z,
			definition.springRotateFactor.x,
			definition.springRotateFactor.y,
			definition.springRotateFactor.z,
		};
		for (int i = 0; i < 6; i++) {
			if (stiffness[i] != 0.0f) {
				jointConstraint->enableSpring(i, true);
				jointConstraint->setStiffness(i, stiffness[i]);
			}
		}
		constraint = std::move(jointConstraint);
	}
}
