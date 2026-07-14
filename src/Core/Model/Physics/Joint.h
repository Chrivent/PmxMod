#pragma once

#include "Core/Parser/PmxParser.h"

#include <memory>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	class RigidBody;

	// 두 강체를 연결하는 Bullet 제약 조건을 생성하고 소유한다.
	class Joint {
		std::unique_ptr<btTypedConstraint> constraint;

	public:
		btTypedConstraint* GetConstraint() const { return constraint.get(); }

		// PMX 조인트 정보를 두 강체 사이의 Bullet 제약으로 생성한다.
		void Create(const PmxParser::PmxJoint& pmxJoint, const RigidBody& rigidBodyA, const RigidBody& rigidBodyB);
	};
}
