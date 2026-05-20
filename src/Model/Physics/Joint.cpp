#include "Joint.h"

#include "RigidBody.h"

namespace Chrivent {
	void Joint::Create(const PmxParser::PmxJoint& pmxJoint, const RigidBodyInfo& rigidBodyA, const RigidBodyInfo& rigidBodyB) {
		constraint = nullptr;
		btMatrix3x3 rotMat;
		rotMat.setEulerZYX(pmxJoint.rotate.x, pmxJoint.rotate.y, pmxJoint.rotate.z);
		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(btVector3(pmxJoint.translate.x, pmxJoint.translate.y, pmxJoint.translate.z));
		transform.setBasis(rotMat);
		btTransform invA = rigidBodyA.rigidBody->getWorldTransform().inverse();
		btTransform invB = rigidBodyB.rigidBody->getWorldTransform().inverse();
		invA = invA * transform;
		invB = invB * transform;
		auto jointConstraint = std::make_unique<btGeneric6DofSpringConstraint>(
			*rigidBodyA.rigidBody, *rigidBodyB.rigidBody,
			invA, invB, true);
		jointConstraint->setLinearLowerLimit(btVector3(pmxJoint.translateLowerLimit.x, pmxJoint.translateLowerLimit.y, pmxJoint.translateLowerLimit.z));
		jointConstraint->setLinearUpperLimit(btVector3(pmxJoint.translateUpperLimit.x, pmxJoint.translateUpperLimit.y, pmxJoint.translateUpperLimit.z));
		jointConstraint->setAngularLowerLimit(btVector3(pmxJoint.rotateLowerLimit.x, pmxJoint.rotateLowerLimit.y, pmxJoint.rotateLowerLimit.z));
		jointConstraint->setAngularUpperLimit(btVector3(pmxJoint.rotateUpperLimit.x, pmxJoint.rotateUpperLimit.y, pmxJoint.rotateUpperLimit.z));
		const float stiffness[6] = {
			pmxJoint.springTranslateFactor.x,
			pmxJoint.springTranslateFactor.y,
			pmxJoint.springTranslateFactor.z,
			pmxJoint.springRotateFactor.x,
			pmxJoint.springRotateFactor.y,
			pmxJoint.springRotateFactor.z,
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
