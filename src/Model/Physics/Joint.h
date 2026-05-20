#pragma once

#include "../../Parser/PmxParser.h"

#include <memory>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	struct RigidBodyInfo;

	class Joint {
		std::unique_ptr<btTypedConstraint> constraint;

	public:
		btTypedConstraint* GetConstraint() const { return constraint.get(); }

		// PMX 조인트 정보를 두 강체 사이의 Bullet 제약으로 생성한다.
		void Create(const PmxParser::PmxJoint& pmxJoint, const RigidBodyInfo& rigidBodyA, const RigidBodyInfo& rigidBodyB);
	};
}
