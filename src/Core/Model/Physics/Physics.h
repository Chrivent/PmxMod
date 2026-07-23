#pragma once

#include <cstdint>
#include <memory>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	// 바닥 프록시는 충돌 그룹과 무관하게 통과시키는 Bullet broadphase 필터다.
	class PhysicsOverlapFilter final : public btOverlapFilterCallback {
		btBroadphaseProxy* unfilteredProxy = nullptr;

	public:
		void SetUnfilteredProxy(btBroadphaseProxy* proxy) { unfilteredProxy = proxy; }

		// 두 broadphase 프록시가 충돌 후보가 될 수 있는지 필터링한다.
		bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;
	};

	// Bullet 물리 월드와 등록된 강체 및 조인트를 관리한다.
	class Physics {
		static constexpr float simulationFps = 120.0f;
		static constexpr int maxSubStepCount = 10;

		std::unique_ptr<btBroadphaseInterface>					broadPhase;
		std::unique_ptr<btDefaultCollisionConfiguration>		collisionConfig;
		std::unique_ptr<btCollisionDispatcher>					dispatcher;
		std::unique_ptr<btSequentialImpulseConstraintSolver>	solver;
		std::unique_ptr<btDiscreteDynamicsWorld>				world;
		std::unique_ptr<btCollisionShape>						groundShape;
		std::unique_ptr<btMotionState>							groundMotionState;
		std::unique_ptr<btRigidBody>							groundRigidBody;
		std::unique_ptr<PhysicsOverlapFilter>					filterCallback;

		// Bullet 월드와 기본 물리 리소스를 생성한다.
		void Create();

	public:
		Physics();
		~Physics();

		// 강체를 충돌 그룹 설정과 함께 물리 월드에 등록한다.
		void AddRigidBody(btRigidBody& rigidBody, uint16_t group, uint16_t groupMask) const;
		// 물리 월드에서 강체를 제거한다.
		void RemoveRigidBody(btRigidBody& rigidBody) const;
		// 조인트 제약 조건을 물리 월드에 등록한다.
		void AddConstraint(btTypedConstraint& constraint) const;
		// 물리 월드에서 조인트 제약 조건을 제거한다.
		void RemoveConstraint(btTypedConstraint& constraint) const;
		// 지정한 경과 시간만큼 물리 시뮬레이션을 진행한다.
		void StepSimulation(float elapsed) const;
		// 강체에 남아 있는 broadphase 충돌 쌍을 제거한다.
		void CleanCollisionPairs(btRigidBody& rigidBody) const;
	};
}
