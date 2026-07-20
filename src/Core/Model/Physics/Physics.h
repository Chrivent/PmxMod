#pragma once

#include "Core/Model/Physics/Joint.h"
#include "Core/Model/Physics/RigidBody.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	// 같은 충돌 그룹 내부의 필터 규칙을 Bullet broadphase에 적용한다.
	class OverlapFilterCallback final : public btOverlapFilterCallback {
		std::vector<btBroadphaseProxy*> nonFilterProxy;

	public:
		// broadphase 충돌 필터를 적용하지 않을 프록시를 등록한다.
		void AddNonFilterProxy(btBroadphaseProxy* proxy) { nonFilterProxy.push_back(proxy); }

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
		std::unique_ptr<btOverlapFilterCallback>				filterCallback;

		// Bullet 월드와 기본 물리 리소스를 생성한다.
		void Create();

	public:
		// Bullet 물리 월드와 기본 바닥 강체를 생성한다.
		Physics();
		// 기본 바닥 강체를 해제한 뒤 물리 월드를 파괴한다.
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
		void Step(float elapsed) const;
		// 강체에 남아 있는 broadphase 충돌 쌍을 제거한다.
		void CleanCollisionPairs(btRigidBody& rigidBody) const;
	};

	// 모델의 Bullet 물리 월드, 강체와 조인트를 함께 소유하고 등록 수명을 관리한다.
	class ModelPhysicsData {
	public:
		std::unique_ptr<Physics> physics;
		std::vector<std::unique_ptr<RigidBody>> rigidBodies;
		std::vector<std::unique_ptr<Joint>> joints;

		~ModelPhysicsData();

		// 물리 월드에서 조인트와 강체를 제거하고 소유 리소스를 해제한다.
		void Reset();
	};
}
