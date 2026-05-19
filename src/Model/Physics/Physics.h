#pragma once

#include <btBulletDynamicsCommon.h>
#include <memory>
#include <vector>

namespace Chrivent {
	class OverlapFilterCallback final : public btOverlapFilterCallback {
	public:
		std::vector<btBroadphaseProxy*> nonFilterProxy;

		// 두 broadphase 프록시가 충돌 후보가 될 수 있는지 필터링한다.
		bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;
	};
	
	class Physics {
		std::unique_ptr<btBroadphaseInterface>					broadPhase;
		std::unique_ptr<btDefaultCollisionConfiguration>			collisionConfig;
		std::unique_ptr<btCollisionDispatcher>					dispatcher;
		std::unique_ptr<btSequentialImpulseConstraintSolver>		solver;
		std::unique_ptr<btCollisionShape>						groundShape;
		std::unique_ptr<btMotionState>							groundMotionState;
		std::unique_ptr<btRigidBody>								groundRigidBody;
		std::unique_ptr<btOverlapFilterCallback>					filterCallback;

	public:
		~Physics();

		std::unique_ptr<btDiscreteDynamicsWorld>	world;
		double										fps = 120.0f;
		int											maxSubStepCount = 10;

		// Bullet 월드와 기본 물리 리소스를 생성한다.
		void Create();
	};
}
