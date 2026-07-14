#pragma once

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
		std::unique_ptr<btBroadphaseInterface>					broadPhase;
		std::unique_ptr<btDefaultCollisionConfiguration>		collisionConfig;
		std::unique_ptr<btCollisionDispatcher>					dispatcher;
		std::unique_ptr<btSequentialImpulseConstraintSolver>	solver;
		std::unique_ptr<btCollisionShape>						groundShape;
		std::unique_ptr<btMotionState>							groundMotionState;
		std::unique_ptr<btRigidBody>							groundRigidBody;
		std::unique_ptr<btOverlapFilterCallback>				filterCallback;

	public:
		~Physics();

		std::unique_ptr<btDiscreteDynamicsWorld> world;
		double fps = 120.0f;
		int maxSubStepCount = 10;

		// Bullet 월드와 기본 물리 리소스를 생성한다.
		void Create();
	};
}
