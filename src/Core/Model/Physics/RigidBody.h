#pragma once

#include "Core/Model/ModelTypes.h"
#include "Core/Model/Physics/MotionState.h"
#include <memory>
#include <glm/glm.hpp>

namespace Chrivent {
	class Node;

	// 런타임 강체 설정을 Bullet 충돌체와 운동 상태로 구성한다.
	class RigidBody {
		std::unique_ptr<btCollisionShape>	shape;
		std::unique_ptr<MotionState>		activeMotionState;
		std::unique_ptr<MotionState>		kinematicMotionState;
		std::unique_ptr<btRigidBody>		rigidBody;
		RigidBodyOperation					operation = RigidBodyOperation::Static;
		std::weak_ptr<Node>					node;
		glm::mat4							offsetMat = glm::mat4(1);
		uint16_t							group = 0;
		uint16_t							groupMask = 0;

	public:
		RigidBody(const RigidBodyDefinition& definition, const std::shared_ptr<Node>& nodePtr);

		btRigidBody& GetRigidBody() const { return *rigidBody; }
		uint16_t GetGroup() const { return group; }
		uint16_t GetGroupMask() const { return groupMask; }

		// 활성 상태에 따라 동적 모션 상태와 키네마틱 모션 상태를 전환한다.
		void ApplyActivation(bool activation) const;
		// 강체 변환을 초기 위치로 재설정한다.
		void ResetTransform() const;
		// 강체의 속도와 누적된 외력을 초기화한다.
		void Reset() const;
		// 물리 계산 결과를 연결된 본 변환에 반영한다.
		void ReflectGlobalTransform() const;
		// PMX 오프셋 기준의 로컬 변환을 계산한다.
		void CalcLocalTransform() const;
		// Bullet 중심 질량 변환을 GLM 글로벌 행렬로 변환해 반환한다.
		glm::mat4 CalcTransform() const;
	};
}
