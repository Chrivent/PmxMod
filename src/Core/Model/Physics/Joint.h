#pragma once

#include "Core/Model/ModelTypes.h"

#include <memory>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	class RigidBody;

	// 두 강체를 연결하는 Bullet 제약 조건을 생성하고 소유한다.
	class Joint {
		std::unique_ptr<btTypedConstraint> constraint;

	public:
		Joint(const JointDefinition& definition, const RigidBody& rigidBodyA, const RigidBody& rigidBodyB);

		btTypedConstraint& ResolveConstraint() const { return *constraint; }
	};
}
