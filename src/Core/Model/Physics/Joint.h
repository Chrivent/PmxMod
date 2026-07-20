#pragma once

#include <memory>
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>

namespace Chrivent {
	class RigidBody;

	// 런타임 조인트 생성에 필요한 연결 위치, 제한 및 스프링 계수를 보관한다.
	struct JointDefinition {
		glm::vec3 translate = glm::vec3(0);
		glm::vec3 rotate = glm::vec3(0);
		glm::vec3 translateLowerLimit = glm::vec3(0);
		glm::vec3 translateUpperLimit = glm::vec3(0);
		glm::vec3 rotateLowerLimit = glm::vec3(0);
		glm::vec3 rotateUpperLimit = glm::vec3(0);
		glm::vec3 springTranslateFactor = glm::vec3(0);
		glm::vec3 springRotateFactor = glm::vec3(0);
	};

	// 두 강체를 연결하는 Bullet 제약 조건을 생성하고 소유한다.
	class Joint {
		std::unique_ptr<btTypedConstraint> constraint;

	public:
		Joint(const JointDefinition& definition, const RigidBody& rigidBodyA, const RigidBody& rigidBodyB);

		btTypedConstraint* GetConstraint() const { return constraint.get(); }
	};
}
