#include "Core/Model/Physics/Physics.h"

#include "Core/Model/Bone/Node.h"

namespace Chrivent {
	bool Physics::OverlapFilterCallback::needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const {
		if (proxy0 == unfilteredProxy || proxy1 == unfilteredProxy)
			return true;
		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask) != 0;
		return collides;
	}

	Physics::Physics() {
		Create();
	}

	Physics::~Physics() {
		world->removeRigidBody(groundRigidBody.get());
	}

	void Physics::AddRigidBody(btRigidBody& rigidBody, const uint16_t group, const uint16_t groupMask) const {
		if (group >= 16)
			return;
		world->addRigidBody(&rigidBody, 1u << group, groupMask);
	}

	void Physics::RemoveRigidBody(btRigidBody& rigidBody) const {
		world->removeRigidBody(&rigidBody);
	}

	void Physics::AddConstraint(btTypedConstraint& constraint) const {
		world->addConstraint(&constraint);
	}

	void Physics::RemoveConstraint(btTypedConstraint& constraint) const {
		world->removeConstraint(&constraint);
	}

	void Physics::Step(const float elapsed) const {
		world->stepSimulation(elapsed, maxSubStepCount, 1.0f / simulationFps);
	}

	void Physics::CleanCollisionPairs(btRigidBody& rigidBody) const {
		if (const auto cache = world->getPairCache(); cache && rigidBody.getBroadphaseHandle()) {
			const auto worldDispatcher = world->getDispatcher();
			cache->cleanProxyFromPairs(rigidBody.getBroadphaseHandle(), worldDispatcher);
		}
	}

	void Physics::Create() {
		broadPhase = std::make_unique<btDbvtBroadphase>();
		collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
		dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfig.get());
		solver = std::make_unique<btSequentialImpulseConstraintSolver>();
		world = std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(), broadPhase.get(), solver.get(), collisionConfig.get());
		world->setGravity(btVector3(0, -9.8f * 10.0f, 0));
		groundShape = std::make_unique<btStaticPlaneShape>(btVector3(0, 1, 0), 0.0f);
		btTransform groundTransform;
		groundTransform.setIdentity();
		groundMotionState = std::make_unique<btDefaultMotionState>(groundTransform);
		btRigidBody::btRigidBodyConstructionInfo groundInfo(0, groundMotionState.get(), groundShape.get(), btVector3(0, 0, 0));
		groundRigidBody = std::make_unique<btRigidBody>(groundInfo);
		world->addRigidBody(groundRigidBody.get());
		filterCallback = std::make_unique<OverlapFilterCallback>();
		filterCallback->SetUnfilteredProxy(groundRigidBody->getBroadphaseProxy());
		world->getPairCache()->setOverlapFilterCallback(filterCallback.get());
	}

	ModelPhysicsData::~ModelPhysicsData() {
		Reset();
	}

	void ModelPhysicsData::ReflectTransforms(const std::vector<std::shared_ptr<Node>>& nodes) const {
		for (const auto& rigidBody : rigidBodies) {
			rigidBody->ReflectGlobalTransform();
			rigidBody->CalcLocalTransform();
		}
		for (const auto& node : nodes) {
			if (node->parent.expired())
				node->UpdateGlobalTransform();
		}
	}

	void ModelPhysicsData::Initialize(const std::vector<RigidBodyDefinition>& rigidBodyDefinitions,
		const std::vector<JointDefinition>& jointDefinitions, const std::vector<std::shared_ptr<Node>>& nodes) {
		Reset();
		if (rigidBodyDefinitions.empty())
			return;
		physics = std::make_unique<Physics>();
		rigidBodies.reserve(rigidBodyDefinitions.size());
		for (const auto& definition : rigidBodyDefinitions) {
			std::shared_ptr<Node> node;
			if (definition.nodeIndex >= 0 &&
				static_cast<std::size_t>(definition.nodeIndex) < nodes.size()) {
				node = nodes[definition.nodeIndex];
			}
			auto rigidBody = std::make_unique<RigidBody>(definition, node);
			physics->AddRigidBody(*rigidBody->GetRigidBody(), rigidBody->GetGroup(), rigidBody->GetGroupMask());
			rigidBodies.emplace_back(std::move(rigidBody));
		}
		joints.reserve(jointDefinitions.size());
		for (const auto& definition : jointDefinitions) {
			if (definition.rigidBodyAIndex < 0 || definition.rigidBodyBIndex < 0 ||
				definition.rigidBodyAIndex == definition.rigidBodyBIndex ||
				static_cast<std::size_t>(definition.rigidBodyAIndex) >= rigidBodies.size() ||
				static_cast<std::size_t>(definition.rigidBodyBIndex) >= rigidBodies.size()) {
				continue;
			}
			auto joint = std::make_unique<Joint>(
				definition, *rigidBodies[definition.rigidBodyAIndex], *rigidBodies[definition.rigidBodyBIndex]);
			physics->AddConstraint(*joint->GetConstraint());
			joints.emplace_back(std::move(joint));
		}
	}

	void ModelPhysicsData::ResetSimulation(const std::vector<std::shared_ptr<Node>>& nodes) const {
		if (!IsInitialized())
			return;
		for (const auto& rigidBody : rigidBodies) {
			rigidBody->ApplyActivation(false);
			rigidBody->ResetTransform();
		}
		physics->Step(1.0f / 60.0f);
		ReflectTransforms(nodes);
		for (const auto& rigidBody : rigidBodies) {
			physics->CleanCollisionPairs(*rigidBody->GetRigidBody());
			rigidBody->Reset();
		}
	}

	void ModelPhysicsData::UpdateSimulation(const float elapsed, const std::vector<std::shared_ptr<Node>>& nodes) const {
		if (!IsInitialized())
			return;
		for (const auto& rigidBody : rigidBodies)
			rigidBody->ApplyActivation(true);
		physics->Step(elapsed);
		ReflectTransforms(nodes);
	}

	void ModelPhysicsData::Reset() {
		if (physics) {
			for (const auto& joint : joints)
				physics->RemoveConstraint(*joint->GetConstraint());
			for (const auto& rigidBody : rigidBodies)
				physics->RemoveRigidBody(*rigidBody->GetRigidBody());
		}
		joints.clear();
		rigidBodies.clear();
		physics.reset();
	}
}
