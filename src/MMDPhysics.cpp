#include "MMDPhysics.h"

#include "MMDNode.h"
#include "MMDModel.h"

#include <glm/gtc/matrix_transform.hpp>

namespace saba
{
	namespace
	{
		glm::mat4 InvZ(const glm::mat4& m) {
			const glm::mat4 invZ = glm::scale(glm::mat4(1), glm::vec3(1, 1, -1));
			return invZ * m * invZ;
		}
	}

	bool MMDFilterCallback::needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const {
		const auto findIt = std::ranges::find_if(m_nonFilterProxy,
			[proxy0, proxy1](const auto &x) { return x == proxy0 || x == proxy1; }
		);
		if (findIt != m_nonFilterProxy.end())
			return true;
		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
		return collides;
	}

	btDiscreteDynamicsWorld * MMDPhysics::GetDynamicsWorld() const {
		return m_world.get();
	}

	//*******************
	// MMDRigidBody
	//*******************

	DefaultMotionState::DefaultMotionState(const glm::mat4& transform) {
		glm::mat4 trans = InvZ(transform);
		m_transform.setFromOpenGLMatrix(&trans[0][0]);
		m_initialTransform = m_transform;
	}

	void DefaultMotionState::getWorldTransform(btTransform& worldTransform) const {
		worldTransform = m_transform;
	}

	void DefaultMotionState::setWorldTransform(const btTransform& worldTransform) {
		m_transform = worldTransform;
	}

	void DefaultMotionState::Reset() {
		m_transform = m_initialTransform;
	}

	void DefaultMotionState::ReflectGlobalTransform() {
	}

	DynamicMotionState::DynamicMotionState(MMDNode* node, const glm::mat4& offset, const bool override)
		: m_node(node)
		, m_offset(offset)
		, m_override(override) {
		m_invOffset = glm::inverse(offset);
		Reset();
	}

	void DynamicMotionState::getWorldTransform(btTransform& worldTransform) const {
		worldTransform = m_transform;
	}

	void DynamicMotionState::setWorldTransform(const btTransform& worldTransform) {
		m_transform = worldTransform;
	}

	void DynamicMotionState::Reset() {
		glm::mat4 global = InvZ(m_node->m_global * m_offset);
		m_transform.setFromOpenGLMatrix(&global[0][0]);
	}

	void DynamicMotionState::ReflectGlobalTransform() {
		alignas(16) glm::mat4 world;
		m_transform.getOpenGLMatrix(&world[0][0]);
		const glm::mat4 btGlobal = InvZ(world) * m_invOffset;

		if (m_override) {
			m_node->m_global = btGlobal;
			m_node->UpdateChildTransform();
		}
	}

	DynamicAndBoneMergeMotionState::DynamicAndBoneMergeMotionState(MMDNode* node, const glm::mat4& offset, const bool override)
		: m_node(node)
		, m_offset(offset)
		, m_override(override) {
		m_invOffset = glm::inverse(offset);
		Reset();
	}

	void DynamicAndBoneMergeMotionState::getWorldTransform(btTransform& worldTransform) const {
		worldTransform = m_transform;
	}

	void DynamicAndBoneMergeMotionState::setWorldTransform(const btTransform& worldTransform) {
		m_transform = worldTransform;
	}

	void DynamicAndBoneMergeMotionState::Reset() {
		glm::mat4 global = InvZ(m_node->m_global * m_offset);
		m_transform.setFromOpenGLMatrix(&global[0][0]);
	}

	void DynamicAndBoneMergeMotionState::ReflectGlobalTransform() {
		alignas(16) glm::mat4 world;
		m_transform.getOpenGLMatrix(&world[0][0]);
		glm::mat4 btGlobal = InvZ(world) * m_invOffset;
		glm::mat4 global = m_node->m_global;
		btGlobal[3] = global[3];

		if (m_override) {
			m_node->m_global = btGlobal;
			m_node->UpdateChildTransform();
		}
	}

	KinematicMotionState::KinematicMotionState(MMDNode* node, const glm::mat4& offset)
		: m_node(node)
		, m_offset(offset) {
	}

	void KinematicMotionState::getWorldTransform(btTransform& worldTransform) const {
		glm::mat4 m;
		if (m_node != nullptr)
			m = m_node->m_global * m_offset;
		else
			m = m_offset;
		m = InvZ(m);
		worldTransform.setFromOpenGLMatrix(&m[0][0]);
	}

	void KinematicMotionState::setWorldTransform(const btTransform& worldTransform) {
	}

	void KinematicMotionState::Reset() {
	}

	void KinematicMotionState::ReflectGlobalTransform() {
	}

	MMDRigidBody::MMDRigidBody()
		: m_rigidBodyType(Operation::Static)
		, m_group(0)
		, m_groupMask(0)
		, m_node(nullptr)
		, m_offsetMat(1) {
	}

	bool MMDRigidBody::Create(const PMXRigidbody& pmxRigidBody, const MMDModel* model, MMDNode * node) {
		Destroy();

		switch (pmxRigidBody.m_shape) {
			case PMXRigidbody::Shape::Sphere:
				m_shape = std::make_unique<btSphereShape>(pmxRigidBody.m_shapeSize.x);
				break;
			case PMXRigidbody::Shape::Box:
				m_shape = std::make_unique<btBoxShape>(btVector3(
					pmxRigidBody.m_shapeSize.x,
					pmxRigidBody.m_shapeSize.y,
					pmxRigidBody.m_shapeSize.z
				));
				break;
			case PMXRigidbody::Shape::Capsule:
				m_shape = std::make_unique<btCapsuleShape>(
					pmxRigidBody.m_shapeSize.x,
					pmxRigidBody.m_shapeSize.y
				);
				break;
			default:
				break;
		}
		if (m_shape == nullptr)
			return false;

		btScalar mass(0.0f);
		btVector3 localInertia(0, 0, 0);
		if (pmxRigidBody.m_op != Operation::Static)
			mass = pmxRigidBody.m_mass;
		if (mass != 0)
			m_shape->calculateLocalInertia(mass, localInertia);

		const auto rx = glm::rotate(glm::mat4(1), pmxRigidBody.m_rotate.x, glm::vec3(1, 0, 0));
		const auto ry = glm::rotate(glm::mat4(1), pmxRigidBody.m_rotate.y, glm::vec3(0, 1, 0));
		const auto rz = glm::rotate(glm::mat4(1), pmxRigidBody.m_rotate.z, glm::vec3(0, 0, 1));
		const glm::mat4 rotMat = ry * rx * rz;
		const glm::mat4 translateMat = glm::translate(glm::mat4(1), pmxRigidBody.m_translate);

		const glm::mat4 rbMat = InvZ(translateMat * rotMat);

		MMDNode *kinematicNode = nullptr;
		if (node != nullptr) {
			m_offsetMat = glm::inverse(node->m_global) * rbMat;
			kinematicNode = node;
		} else {
			const MMDNode *root = model->m_nodeMan.GetNodeByIndex(0);
			m_offsetMat = glm::inverse(root->m_global) * rbMat;
			kinematicNode = model->m_nodeMan.GetNodeByIndex(0);
		}

		btMotionState *MMDMotionState = nullptr;
		if (pmxRigidBody.m_op == Operation::Static) {
			m_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, m_offsetMat);
			MMDMotionState = m_kinematicMotionState.get();
		} else if (node != nullptr) {
			if (pmxRigidBody.m_op == Operation::Dynamic) {
				m_activeMotionState = std::make_unique<DynamicMotionState>(kinematicNode, m_offsetMat);
				m_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, m_offsetMat);
				MMDMotionState = m_activeMotionState.get();
			} else if (pmxRigidBody.m_op == Operation::DynamicAndBoneMerge) {
				m_activeMotionState = std::make_unique<DynamicAndBoneMergeMotionState>(kinematicNode, m_offsetMat);
				m_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, m_offsetMat);
				MMDMotionState = m_activeMotionState.get();
			}
		} else {
			m_activeMotionState = std::make_unique<DefaultMotionState>(m_offsetMat);
			m_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, m_offsetMat);
			MMDMotionState = m_activeMotionState.get();
		}

		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, MMDMotionState, m_shape.get(), localInertia);
		rbInfo.m_linearDamping = pmxRigidBody.m_translateDimmer;
		rbInfo.m_angularDamping = pmxRigidBody.m_rotateDimmer;
		rbInfo.m_restitution = pmxRigidBody.m_repulsion;
		rbInfo.m_friction = pmxRigidBody.m_friction;
		rbInfo.m_additionalDamping = true;

		m_rigidBody = std::make_unique<btRigidBody>(rbInfo);
		m_rigidBody->setUserPointer(this);
		m_rigidBody->setSleepingThresholds(0.01f, glm::radians(0.1f));
		m_rigidBody->setActivationState(DISABLE_DEACTIVATION);
		if (pmxRigidBody.m_op == Operation::Static)
			m_rigidBody->setCollisionFlags(m_rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);

		m_rigidBodyType = pmxRigidBody.m_op;
		m_group = pmxRigidBody.m_group;
		m_groupMask = pmxRigidBody.m_collisionGroup;
		m_node = node;
		m_name = pmxRigidBody.m_name;

		return true;
	}

	void MMDRigidBody::Destroy() {
		m_shape = nullptr;
	}

	btRigidBody * MMDRigidBody::GetRigidBody() const {
		return m_rigidBody.get();
	}

	void MMDRigidBody::SetActivation(const bool activation) const {
		if (m_rigidBodyType != Operation::Static) {
			if (activation) {
				m_rigidBody->setCollisionFlags(
					m_rigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
				m_rigidBody->setMotionState(m_activeMotionState.get());
			} else {
				m_rigidBody->setCollisionFlags(
					m_rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
				m_rigidBody->setMotionState(m_kinematicMotionState.get());
			}
		} else
			m_rigidBody->setMotionState(m_kinematicMotionState.get());
	}

	void MMDRigidBody::ResetTransform() const {
		if (m_activeMotionState != nullptr)
			m_activeMotionState->Reset();
	}

	void MMDRigidBody::Reset(const MMDPhysics* physics) const {
		const auto cache = physics->GetDynamicsWorld()->getPairCache();
		if (cache != nullptr) {
			const auto dispatcher = physics->GetDynamicsWorld()->getDispatcher();
			cache->cleanProxyFromPairs(m_rigidBody->getBroadphaseHandle(), dispatcher);
		}
		m_rigidBody->setAngularVelocity(btVector3(0, 0, 0));
		m_rigidBody->setLinearVelocity(btVector3(0, 0, 0));
		m_rigidBody->clearForces();
	}

	void MMDRigidBody::ReflectGlobalTransform() const {
		if (m_activeMotionState != nullptr)
			m_activeMotionState->ReflectGlobalTransform();
		if (m_kinematicMotionState != nullptr)
			m_kinematicMotionState->ReflectGlobalTransform();
	}

	void MMDRigidBody::CalcLocalTransform() const {
		if (m_node != nullptr) {
			const auto parent = m_node->m_parent;
			if (parent != nullptr) {
				const auto local = glm::inverse(parent->m_global) * m_node->m_global;
				m_node->m_local = local;
			} else
				m_node->m_local = m_node->m_global;
		}
	}

	glm::mat4 MMDRigidBody::GetTransform() const {
		const btTransform transform = m_rigidBody->getCenterOfMassTransform();
		alignas(16) glm::mat4 mat;
		transform.getOpenGLMatrix(&mat[0][0]);
		return InvZ(mat);
	}


	//*******************
	// MMDJoint
	//*******************

	bool MMDJoint::CreateJoint(const PMXJoint& pmxJoint, const MMDRigidBody* rigidBodyA, const MMDRigidBody* rigidBodyB) {
		Destroy();

		btMatrix3x3 rotMat;
		rotMat.setEulerZYX(pmxJoint.m_rotate.x, pmxJoint.m_rotate.y, pmxJoint.m_rotate.z);

		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(btVector3(
			pmxJoint.m_translate.x,
			pmxJoint.m_translate.y,
			pmxJoint.m_translate.z
		));
		transform.setBasis(rotMat);

		btTransform invA = rigidBodyA->GetRigidBody()->getWorldTransform().inverse();
		btTransform invB = rigidBodyB->GetRigidBody()->getWorldTransform().inverse();
		invA = invA * transform;
		invB = invB * transform;

		auto constraint = std::make_unique<btGeneric6DofSpringConstraint>(
			*rigidBodyA->GetRigidBody(),
			*rigidBodyB->GetRigidBody(),
			invA,
			invB,
			true);
		constraint->setLinearLowerLimit(btVector3(
			pmxJoint.m_translateLowerLimit.x,
			pmxJoint.m_translateLowerLimit.y,
			pmxJoint.m_translateLowerLimit.z
		));
		constraint->setLinearUpperLimit(btVector3(
			pmxJoint.m_translateUpperLimit.x,
			pmxJoint.m_translateUpperLimit.y,
			pmxJoint.m_translateUpperLimit.z
		));

		constraint->setAngularLowerLimit(btVector3(
			pmxJoint.m_rotateLowerLimit.x,
			pmxJoint.m_rotateLowerLimit.y,
			pmxJoint.m_rotateLowerLimit.z
		));
		constraint->setAngularUpperLimit(btVector3(
			pmxJoint.m_rotateUpperLimit.x,
			pmxJoint.m_rotateUpperLimit.y,
			pmxJoint.m_rotateUpperLimit.z
		));

		if (pmxJoint.m_springTranslateFactor.x != 0) {
			constraint->enableSpring(0, true);
			constraint->setStiffness(0, pmxJoint.m_springTranslateFactor.x);
		}
		if (pmxJoint.m_springTranslateFactor.y != 0) {
			constraint->enableSpring(1, true);
			constraint->setStiffness(1, pmxJoint.m_springTranslateFactor.y);
		}
		if (pmxJoint.m_springTranslateFactor.z != 0) {
			constraint->enableSpring(2, true);
			constraint->setStiffness(2, pmxJoint.m_springTranslateFactor.z);
		}
		if (pmxJoint.m_springRotateFactor.x != 0) {
			constraint->enableSpring(3, true);
			constraint->setStiffness(3, pmxJoint.m_springRotateFactor.x);
		}
		if (pmxJoint.m_springRotateFactor.y != 0) {
			constraint->enableSpring(4, true);
			constraint->setStiffness(4, pmxJoint.m_springRotateFactor.y);
		}
		if (pmxJoint.m_springRotateFactor.z != 0) {
			constraint->enableSpring(5, true);
			constraint->setStiffness(5, pmxJoint.m_springRotateFactor.z);
		}

		m_constraint = std::move(constraint);

		return true;
	}

	void MMDJoint::Destroy() {
		m_constraint = nullptr;
	}

	btTypedConstraint* MMDJoint::GetConstraint() const {
		return m_constraint.get();
	}

	MMDPhysics::MMDPhysics()
		: m_fps(120.0f)
		, m_maxSubStepCount(10) {
	}

	MMDPhysics::~MMDPhysics() {
		Destroy();
	}

	void MMDPhysics::Create() {
		m_broadPhase = std::make_unique<btDbvtBroadphase>();
		m_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
		m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfig.get());

		m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();

		m_world = std::make_unique<btDiscreteDynamicsWorld>(
			m_dispatcher.get(),
			m_broadPhase.get(),
			m_solver.get(),
			m_collisionConfig.get()
		);

		m_world->setGravity(btVector3(0, -9.8f * 10.0f, 0));

		m_groundShape = std::make_unique<btStaticPlaneShape>(btVector3(0, 1, 0), 0.0f);

		btTransform groundTransform;
		groundTransform.setIdentity();

		m_groundMS = std::make_unique<btDefaultMotionState>(groundTransform);

		btRigidBody::btRigidBodyConstructionInfo groundInfo(0, m_groundMS.get(), m_groundShape.get(), btVector3(0, 0, 0));
		m_groundRB = std::make_unique<btRigidBody>(groundInfo);

		m_world->addRigidBody(m_groundRB.get());

		auto filterCB = std::make_unique<MMDFilterCallback>();
		filterCB->m_nonFilterProxy.push_back(m_groundRB->getBroadphaseProxy());
		m_world->getPairCache()->setOverlapFilterCallback(filterCB.get());
		m_filterCB = std::move(filterCB);
	}

	void MMDPhysics::Destroy() {
		if (m_world != nullptr && m_groundRB != nullptr)
			m_world->removeRigidBody(m_groundRB.get());

		m_broadPhase = nullptr;
		m_collisionConfig = nullptr;
		m_dispatcher = nullptr;
		m_solver = nullptr;
		m_world = nullptr;
		m_groundShape = nullptr;
		m_groundMS = nullptr;
		m_groundRB = nullptr;
	}

	void MMDPhysics::Update(const float time) const {
		if (m_world != nullptr)
			m_world->stepSimulation(time, m_maxSubStepCount, static_cast<btScalar>(1.0 / m_fps));
	}

	void MMDPhysics::AddRigidBody(const MMDRigidBody* mmdRB) const {
		m_world->addRigidBody(
			mmdRB->GetRigidBody(),
			1 << mmdRB->m_group,
			mmdRB->m_groupMask
		);
	}

	void MMDPhysics::RemoveRigidBody(const MMDRigidBody* mmdRB) const {
		m_world->removeRigidBody(mmdRB->GetRigidBody());
	}

	void MMDPhysics::AddJoint(const MMDJoint* mmdJoint) const {
		if (mmdJoint->GetConstraint() != nullptr)
			m_world->addConstraint(mmdJoint->GetConstraint());
	}

	void MMDPhysics::RemoveJoint(const MMDJoint* mmdJoint) const {
		if (mmdJoint->GetConstraint() != nullptr)
			m_world->removeConstraint(mmdJoint->GetConstraint());
	}

	MMDPhysicsManager::~MMDPhysicsManager() {
		for (auto &joint: m_joints)
			m_mmdPhysics->RemoveJoint(joint.get());
		m_joints.clear();

		for (auto &rb: m_rigidBodies)
			m_mmdPhysics->RemoveRigidBody(rb.get());
		m_rigidBodies.clear();

		m_mmdPhysics.reset();
	}

	void MMDPhysicsManager::Create() {
		m_mmdPhysics = std::make_unique<MMDPhysics>();
		m_mmdPhysics->Create();
	}

	MMDPhysics* MMDPhysicsManager::GetMMDPhysics() const {
		return m_mmdPhysics.get();
	}

	MMDRigidBody* MMDPhysicsManager::AddRigidBody() {
		auto rigidBody = std::make_unique<MMDRigidBody>();
		m_rigidBodies.emplace_back(std::move(rigidBody));

		return m_rigidBodies.back().get();
	}

	MMDJoint* MMDPhysicsManager::AddJoint() {
		auto joint = std::make_unique<MMDJoint>();
		m_joints.emplace_back(std::move(joint));

		return m_joints.back().get();
	}
}
