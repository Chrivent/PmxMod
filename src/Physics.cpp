#include "Physics.h"

#include "Model.h"
#include "Util.h"

bool OverlapFilterCallback::needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const {
	const auto endIt = m_nonFilterProxy.end();
	if (std::ranges::find(m_nonFilterProxy, proxy0) != endIt || std::ranges::find(m_nonFilterProxy, proxy1) != endIt)
		return true;
	bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
	collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask) != 0;
	return collides;
}

DefaultMotionState::DefaultMotionState(const glm::mat4& transform) {
	glm::mat4 trans = Util::InvZ(transform);
	m_transform.setFromOpenGLMatrix(&trans[0][0]);
	m_initialTransform = m_transform;
}

DynamicMotionState::DynamicMotionState(Node* node, const glm::mat4& offset)
	: m_node(node)
	, m_offset(offset) {
	m_invOffset = glm::inverse(offset);
	DynamicMotionState::Reset();
}

void DynamicMotionState::Reset() {
	glm::mat4 global = Util::InvZ(m_node->m_global * m_offset);
	m_transform.setFromOpenGLMatrix(&global[0][0]);
}

void DynamicMotionState::ReflectGlobalTransform() {
	glm::mat4 world;
	m_transform.getOpenGLMatrix(&world[0][0]);
	glm::mat4 btGlobal = Util::InvZ(world) * m_invOffset;
	PostProcessBtGlobal(btGlobal);
	m_node->m_global = btGlobal;
	m_node->UpdateChildTransform();
}

void DynamicAndBoneMergeMotionState::PostProcessBtGlobal(glm::mat4& btGlobal) const {
	btGlobal[3] = m_node->m_global[3];
}

KinematicMotionState::KinematicMotionState(Node* node, const glm::mat4& offset)
	: m_node(node)
	, m_offset(offset) {
}

void KinematicMotionState::getWorldTransform(btTransform& worldTransform) const {
	glm::mat4 global = Util::InvZ(m_node->m_global * m_offset);
	worldTransform.setFromOpenGLMatrix(&global[0][0]);
}

void RigidBody::Create(const PMXReader::PMXRigidbody& pmxRigidBody, const Model* model, Node * node) {
	switch (pmxRigidBody.m_shape) {
		case Shape::Sphere:
			m_shape = std::make_unique<btSphereShape>(pmxRigidBody.m_shapeSize.x);
			break;
		case Shape::Box:
			m_shape = std::make_unique<btBoxShape>(btVector3(
				pmxRigidBody.m_shapeSize.x,
				pmxRigidBody.m_shapeSize.y,
				pmxRigidBody.m_shapeSize.z
			));
			break;
		case Shape::Capsule:
			m_shape = std::make_unique<btCapsuleShape>(
				pmxRigidBody.m_shapeSize.x,
				pmxRigidBody.m_shapeSize.y
			);
			break;
	}
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
	const glm::mat4 rbMat = Util::InvZ(translateMat * rotMat);
	auto* kinematicNode = node ? node : model->m_nodes[0].get();
	m_offsetMat = glm::inverse(kinematicNode->m_global) * rbMat;
	m_kinematicMotionState = std::make_unique<KinematicMotionState>(kinematicNode, m_offsetMat);
	if (pmxRigidBody.m_op != Operation::Static) {
		if (node) {
			if (pmxRigidBody.m_op == Operation::Dynamic)
				m_activeMotionState = std::make_unique<DynamicMotionState>(kinematicNode, m_offsetMat);
			else
				m_activeMotionState = std::make_unique<DynamicAndBoneMergeMotionState>(kinematicNode, m_offsetMat);
		} else
			m_activeMotionState = std::make_unique<DefaultMotionState>(m_offsetMat);
	}
	btMotionState* motionState = m_activeMotionState ? m_activeMotionState.get() : m_kinematicMotionState.get();
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, m_shape.get(), localInertia);
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
}

void RigidBody::SetActivation(const bool activation) const {
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

void RigidBody::ResetTransform() const {
	if (m_activeMotionState)
		m_activeMotionState->Reset();
}

void RigidBody::Reset(const Physics* physics) const {
	if (const auto cache = physics->m_world->getPairCache()) {
		const auto dispatcher = physics->m_world->getDispatcher();
		cache->cleanProxyFromPairs(m_rigidBody->getBroadphaseHandle(), dispatcher);
	}
	m_rigidBody->setAngularVelocity(btVector3(0, 0, 0));
	m_rigidBody->setLinearVelocity(btVector3(0, 0, 0));
	m_rigidBody->clearForces();
}

void RigidBody::ReflectGlobalTransform() const {
	if (m_activeMotionState)
		m_activeMotionState->ReflectGlobalTransform();
	if (m_kinematicMotionState)
		m_kinematicMotionState->ReflectGlobalTransform();
}

void RigidBody::CalcLocalTransform() const {
	if (m_node) {
		if (const auto parent = m_node->m_parent) {
			const auto local = glm::inverse(parent->m_global) * m_node->m_global;
			m_node->m_local = local;
		} else
			m_node->m_local = m_node->m_global;
	}
}

glm::mat4 RigidBody::GetTransform() const {
	const btTransform transform = m_rigidBody->getCenterOfMassTransform();
	glm::mat4 mat;
	transform.getOpenGLMatrix(&mat[0][0]);
	return Util::InvZ(mat);
}

void Joint::CreateJoint(const PMXReader::PMXJoint& pmxJoint, const RigidBody* rigidBodyA, const RigidBody* rigidBodyB) {
	m_constraint = nullptr;
	btMatrix3x3 rotMat;
	rotMat.setEulerZYX(pmxJoint.m_rotate.x, pmxJoint.m_rotate.y, pmxJoint.m_rotate.z);
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(pmxJoint.m_translate.x, pmxJoint.m_translate.y, pmxJoint.m_translate.z));
	transform.setBasis(rotMat);
	btTransform invA = rigidBodyA->m_rigidBody->getWorldTransform().inverse();
	btTransform invB = rigidBodyB->m_rigidBody->getWorldTransform().inverse();
	invA = invA * transform;
	invB = invB * transform;
	auto constraint = std::make_unique<btGeneric6DofSpringConstraint>(
		*rigidBodyA->m_rigidBody, *rigidBodyB->m_rigidBody,
		invA, invB, true);
	constraint->setLinearLowerLimit(btVector3(pmxJoint.m_translateLowerLimit.x, pmxJoint.m_translateLowerLimit.y, pmxJoint.m_translateLowerLimit.z));
	constraint->setLinearUpperLimit(btVector3(pmxJoint.m_translateUpperLimit.x, pmxJoint.m_translateUpperLimit.y, pmxJoint.m_translateUpperLimit.z));
	constraint->setAngularLowerLimit(btVector3(pmxJoint.m_rotateLowerLimit.x, pmxJoint.m_rotateLowerLimit.y, pmxJoint.m_rotateLowerLimit.z));
	constraint->setAngularUpperLimit(btVector3(pmxJoint.m_rotateUpperLimit.x, pmxJoint.m_rotateUpperLimit.y, pmxJoint.m_rotateUpperLimit.z));
	const float stiffness[6] = {
		pmxJoint.m_springTranslateFactor.x,
		pmxJoint.m_springTranslateFactor.y,
		pmxJoint.m_springTranslateFactor.z,
		pmxJoint.m_springRotateFactor.x,
		pmxJoint.m_springRotateFactor.y,
		pmxJoint.m_springRotateFactor.z,
	};
	for (int i = 0; i < 6; i++) {
		if (stiffness[i] != 0.0f) {
			constraint->enableSpring(i, true);
			constraint->setStiffness(i, stiffness[i]);
		}
	}
	m_constraint = std::move(constraint);
}

Physics::~Physics() {
	if (m_world && m_groundRB)
		m_world->removeRigidBody(m_groundRB.get());
}

void Physics::Create() {
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
	auto filterCB = std::make_unique<OverlapFilterCallback>();
	filterCB->m_nonFilterProxy.push_back(m_groundRB->getBroadphaseProxy());
	m_world->getPairCache()->setOverlapFilterCallback(filterCB.get());
	m_filterCB = std::move(filterCB);
}
