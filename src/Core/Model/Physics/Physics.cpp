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

	std::expected<void, PhysicsError> ModelPhysicsData::ValidateDefinitions(
		const std::vector<RigidBodyDefinition>& rigidBodyDefinitions,
		const std::vector<JointDefinition>& jointDefinitions, const std::size_t nodeCount) {
		const auto IsFinite = [](const glm::vec3& value) {
			return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
		};
		for (std::size_t index = 0; index < rigidBodyDefinitions.size(); index++) {
			const auto& definition = rigidBodyDefinitions[index];
			const auto Fail = [index](const std::string& message) {
				return std::unexpected(PhysicsError{PhysicsErrorCode::InvalidRigidBody, index, message});
			};
			if (definition.nodeIndex < -1 ||
				(definition.nodeIndex >= 0 && static_cast<std::size_t>(definition.nodeIndex) >= nodeCount))
				return Fail("강체가 존재하지 않는 모델 노드를 참조합니다.");
			if (definition.group >= 16)
				return Fail("강체 충돌 그룹은 0부터 15 사이여야 합니다.");
			switch (definition.shape) {
				case RigidBodyShape::Sphere:
					if (!(definition.shapeSize.x > 0.0f))
						return Fail("구 강체의 반지름은 0보다 커야 합니다.");
					break;
				case RigidBodyShape::Box:
					if (!(definition.shapeSize.x > 0.0f && definition.shapeSize.y > 0.0f &&
						definition.shapeSize.z > 0.0f))
						return Fail("상자 강체의 각 반길이는 0보다 커야 합니다.");
					break;
				case RigidBodyShape::Capsule:
					if (!(definition.shapeSize.x > 0.0f && definition.shapeSize.y >= 0.0f))
						return Fail("캡슐 강체의 반지름은 0보다 크고 높이는 음수가 아니어야 합니다.");
					break;
				default:
					return Fail("지원하지 않는 강체 형상입니다.");
			}
			if (!IsFinite(definition.shapeSize) || !IsFinite(definition.translate) || !IsFinite(definition.rotate))
				return Fail("강체 형상 또는 변환에 유한하지 않은 값이 있습니다.");
			if (!std::isfinite(definition.mass) || !std::isfinite(definition.translateDamping) ||
				!std::isfinite(definition.rotateDamping) || !std::isfinite(definition.restitution) ||
				!std::isfinite(definition.friction) || definition.mass < 0.0f ||
				definition.translateDamping < 0.0f || definition.rotateDamping < 0.0f ||
				definition.restitution < 0.0f || definition.friction < 0.0f)
				return Fail("강체 물성은 유한한 음이 아닌 값이어야 합니다.");
			switch (definition.operation) {
				case RigidBodyOperation::Static:
				case RigidBodyOperation::Dynamic:
				case RigidBodyOperation::DynamicAndBoneMerge:
					break;
				default:
					return Fail("지원하지 않는 강체 동작 방식입니다.");
			}
		}
		for (std::size_t index = 0; index < jointDefinitions.size(); index++) {
			const auto& definition = jointDefinitions[index];
			const auto Fail = [index](const std::string& message) {
				return std::unexpected(PhysicsError{PhysicsErrorCode::InvalidJoint, index, message});
			};
			if (definition.rigidBodyAIndex < 0 || definition.rigidBodyBIndex < 0 ||
				definition.rigidBodyAIndex == definition.rigidBodyBIndex ||
				static_cast<std::size_t>(definition.rigidBodyAIndex) >= rigidBodyDefinitions.size() ||
				static_cast<std::size_t>(definition.rigidBodyBIndex) >= rigidBodyDefinitions.size())
				return Fail("조인트가 유효한 서로 다른 두 강체를 참조해야 합니다.");
			if (!IsFinite(definition.translate) || !IsFinite(definition.rotate) ||
				!IsFinite(definition.translateLowerLimit) || !IsFinite(definition.translateUpperLimit) ||
				!IsFinite(definition.rotateLowerLimit) || !IsFinite(definition.rotateUpperLimit) ||
				!IsFinite(definition.springTranslateFactor) || !IsFinite(definition.springRotateFactor))
				return Fail("조인트 변환, 제한 또는 스프링 값에 유한하지 않은 값이 있습니다.");
		}
		return {};
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

	std::expected<void, PhysicsError> ModelPhysicsData::Initialize(
		const std::vector<RigidBodyDefinition>& rigidBodyDefinitions,
		const std::vector<JointDefinition>& jointDefinitions, const std::vector<std::shared_ptr<Node>>& nodes) {
		const auto validationResult = ValidateDefinitions(rigidBodyDefinitions, jointDefinitions, nodes.size());
		if (!validationResult)
			return std::unexpected(validationResult.error());
		Reset();
		if (rigidBodyDefinitions.empty())
			return {};
		physics = std::make_unique<Physics>();
		rigidBodies.reserve(rigidBodyDefinitions.size());
		for (const auto& definition : rigidBodyDefinitions) {
			std::shared_ptr<Node> node = definition.nodeIndex >= 0 ? nodes[definition.nodeIndex] : nullptr;
			auto rigidBody = std::make_unique<RigidBody>(definition, node);
			physics->AddRigidBody(rigidBody->GetRigidBody(), rigidBody->GetGroup(), rigidBody->GetGroupMask());
			rigidBodies.emplace_back(std::move(rigidBody));
		}
		joints.reserve(jointDefinitions.size());
		for (const auto& definition : jointDefinitions) {
			auto joint = std::make_unique<Joint>(
				definition, *rigidBodies[definition.rigidBodyAIndex], *rigidBodies[definition.rigidBodyBIndex]);
			physics->AddConstraint(joint->GetConstraint());
			joints.emplace_back(std::move(joint));
		}
		return {};
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
			physics->CleanCollisionPairs(rigidBody->GetRigidBody());
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
				physics->RemoveConstraint(joint->GetConstraint());
			for (const auto& rigidBody : rigidBodies)
				physics->RemoveRigidBody(rigidBody->GetRigidBody());
		}
		joints.clear();
		rigidBodies.clear();
		physics.reset();
	}
}
