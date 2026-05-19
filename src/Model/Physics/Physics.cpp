#include "Physics.h"

#include <vector>

namespace Chrivent {
	bool OverlapFilterCallback::needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const {
		const auto endIt = nonFilterProxy.end();
		if (std::ranges::find(nonFilterProxy, proxy0) != endIt || std::ranges::find(nonFilterProxy, proxy1) != endIt)
			return true;
		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask) != 0;
		return collides;
	}

	Physics::~Physics() {
		if (world && groundRigidBody)
			world->removeRigidBody(groundRigidBody.get());
	}

	void Physics::Create() {
		broadPhase = std::make_unique<btDbvtBroadphase>();
		collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
		dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfig.get());
		solver = std::make_unique<btSequentialImpulseConstraintSolver>();
		world = std::make_unique<btDiscreteDynamicsWorld>(
			dispatcher.get(),
			broadPhase.get(),
			solver.get(),
			collisionConfig.get()
		);
		world->setGravity(btVector3(0, -9.8f * 10.0f, 0));
		groundShape = std::make_unique<btStaticPlaneShape>(btVector3(0, 1, 0), 0.0f);
		btTransform groundTransform;
		groundTransform.setIdentity();
		groundMotionState = std::make_unique<btDefaultMotionState>(groundTransform);
		btRigidBody::btRigidBodyConstructionInfo groundInfo(0, groundMotionState.get(), groundShape.get(), btVector3(0, 0, 0));
		groundRigidBody = std::make_unique<btRigidBody>(groundInfo);
		world->addRigidBody(groundRigidBody.get());
		auto filter = std::make_unique<OverlapFilterCallback>();
		filter->nonFilterProxy.push_back(groundRigidBody->getBroadphaseProxy());
		world->getPairCache()->setOverlapFilterCallback(filter.get());
		filterCallback = std::move(filter);
	}
}
