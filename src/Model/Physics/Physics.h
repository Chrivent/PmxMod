#pragma once

#include <memory>
#include <vector>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	struct PhysicsInfo {
		std::unique_ptr<btDiscreteDynamicsWorld>	world;
		double										fps = 120.0f;
		int											maxSubStepCount = 10;
	};

	class OverlapFilterCallback final : public btOverlapFilterCallback {
		std::vector<btBroadphaseProxy*> nonFilterProxy;

	public:
		// broadphase 충돌 필터를 적용하지 않을 프록시를 등록한다.
		void AddNonFilterProxy(btBroadphaseProxy* proxy) { nonFilterProxy.push_back(proxy); }

		// 두 broadphase 프록시가 충돌 후보가 될 수 있는지 필터링한다.
		bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;
	};
	
	class Physics {
		PhysicsInfo info;
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

		PhysicsInfo& GetInfo() { return info; }

		// Bullet 월드와 기본 물리 리소스를 생성한다.
		void Create();
	};
}
