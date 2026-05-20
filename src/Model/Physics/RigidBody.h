#pragma once

#include "MotionState.h"
#include "../../Parser/PmxParser.h"

namespace Chrivent {
	class Node;
	struct PhysicsInfo;
	struct Model;

	struct RigidBodyInfo {
		std::unique_ptr<btRigidBody>	rigidBody;
		uint16_t						group = 0;
		uint16_t						groupMask = 0;
	};

	class RigidBody {
		RigidBodyInfo info;
		std::unique_ptr<btCollisionShape>	shape;
		std::unique_ptr<MotionState>		activeMotionState;
		std::unique_ptr<MotionState>		kinematicMotionState;
		Operation							rigidBodyType = Operation::Static;
		std::weak_ptr<Node>					node;
		glm::mat4							offsetMat = glm::mat4(1);
		std::string							name;

	public:
		RigidBodyInfo& GetInfo() { return info; }

		// PMX 강체 정보를 Bullet 강체와 모션 상태로 생성한다.
		void Create(const PmxParser::PmxRigidbody& pmxRigidBody, const Model* model, const std::shared_ptr<Node>& nodePtr);
		// 활성 상태에 따라 동적 모션 상태와 키네마틱 모션 상태를 전환한다.
		void ApplyActivation(bool activation) const;
		// 강체 변환을 초기 위치로 재설정한다.
		void ResetTransform() const;
		// 물리 월드에 등록된 강체 상태를 초기화한다.
		void Reset(const PhysicsInfo& physicsInfo) const;
		// 물리 계산 결과를 연결된 본 변환에 반영한다.
		void ReflectGlobalTransform() const;
		// PMX 오프셋 기준의 로컬 변환을 계산한다.
		void CalcLocalTransform() const;
		// Bullet 중심 질량 변환을 GLM 글로벌 행렬로 변환해 반환한다.
		glm::mat4 CalcTransform() const;
	};
}
