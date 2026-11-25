#pragma once

#include "Core/Model/ModelTypes.h"
#include "Core/Model/Physics/MotionState.h"

#include <memory>

namespace Chrivent {
	class Node;

	// 런타임 강체 설정을 Bullet 충돌체와 운동 상태로 구성한다.
	class RigidBody {
		std::unique_ptr<btCollisionShape>	shape;
		std::unique_ptr<MotionState>		motionState;
		std::unique_ptr<btRigidBody>		rigidBody;
		std::size_t							poseDepth = 0;
		const Node*							dynamicNode = nullptr;
		btVector3							dynamicInertia{0, 0, 0};
		btScalar							dynamicMass = 0;
		bool								followsAnimation = false;

	public:
		RigidBody(const RigidBodyDefinition& definition, Node* sourceNode, float scale);

		btRigidBody& ResolveRigidBody() const { return *rigidBody; }
		std::size_t GetPoseDepth() const { return poseDepth; }

		// 현재 포즈로 순간 이동하고 적분 및 렌더 보간에 남아 있는 운동량을 모두 지운다.
		void ResetTransform() const;
		// 키로 물리를 끈 본은 충돌체를 유지한 채 애니메이션을 따라간다.
		void SynchronizeAnimationMode();
		// 물리 계산 결과를 연결된 본 변환에 반영한다.
		void ReflectGlobalTransform() const;
	};
}
