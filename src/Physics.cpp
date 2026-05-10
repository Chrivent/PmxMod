#include "Physics.h"

#include "Model.h"
#include "Util.h"

bool OverlapFilterCallback::needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const {
	const auto endIt = nonFilterProxy.end();
	if (std::ranges::find(nonFilterProxy, proxy0) != endIt || std::ranges::find(nonFilterProxy, proxy1) != endIt)
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

DynamicMotionState::DynamicMotionState(const std::shared_ptr<Node>& node, const glm::mat4& offset)
	: m_node(node)
	, m_offset(offset) {
	m_invOffset = glm::inverse(offset);
	DynamicMotionState::Reset();
}

void DynamicMotionState::Reset() {
	const auto node = m_node.lock();
	if (!node)
		return;
	glm::mat4 global = Util::InvZ(node->global * m_offset);
	m_transform.setFromOpenGLMatrix(&global[0][0]);
}

void DynamicMotionState::ReflectGlobalTransform() {
	const auto node = m_node.lock();
	if (!node)
		return;
	glm::mat4 worldTransformMat;
	m_transform.getOpenGLMatrix(&worldTransformMat[0][0]);
	glm::mat4 btGlobal = Util::InvZ(worldTransformMat) * m_invOffset;
	PostProcessBtGlobal(btGlobal);
	node->global = btGlobal;
	node->UpdateChildTransform();
}

void DynamicAndBoneMergeMotionState::PostProcessBtGlobal(glm::mat4& btGlobal) const {
	if (const auto node = m_node.lock())
		btGlobal[3] = node->global[3];
}

KinematicMotionState::KinematicMotionState(const std::shared_ptr<Node>& node, const glm::mat4& offset)
	: m_node(node)
	, m_offset(offset) {
}

void KinematicMotionState::getWorldTransform(btTransform& worldTransform) const {
	const auto node = m_node.lock();
	if (!node)
		return;
	glm::mat4 global = Util::InvZ(node->global * m_offset);
	worldTransform.setFromOpenGLMatrix(&global[0][0]);
}

void RigidBody::Create(const PMXReader::PMXRigidbody& pmxRigidBody, const Model* model, const std::shared_ptr<Node>& node) {
	switch (pmxRigidBody.shape) {
		case Shape::Sphere:
			m_shape = std::make_unique<btSphereShape>(pmxRigidBody.shapeSize.x);
			break;
		case Shape::Box:
			m_shape = std::make_unique<btBoxShape>(btVector3(
				pmxRigidBody.shapeSize.x,
				pmxRigidBody.shapeSize.y,
				pmxRigidBody.shapeSize.z
			));
			break;
		case Shape::Capsule:
			m_shape = std::make_unique<btCapsuleShape>(
				pmxRigidBody.shapeSize.x,
				pmxRigidBody.shapeSize.y
			);
			break;
	}
	btScalar mass(0.0f);
	btVector3 localInertia(0, 0, 0);
	if (pmxRigidBody.op != Operation::Static)
		mass = pmxRigidBody.mass;
	if (mass != 0)
		m_shape->calculateLocalInertia(mass, localInertia);
	const auto rx = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.x, glm::vec3(1, 0, 0));
	const auto ry = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.y, glm::vec3(0, 1, 0));
	const auto rz = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.z, glm::vec3(0, 0, 1));
	const glm::mat4 rotMat = ry * rx * rz;
	const glm::mat4 translateMat = glm::translate(glm::mat4(1), pmxRigidBody.translate);
	const glm::mat4 rbMat = Util::InvZ(translateMat * rotMat);
	m_offsetMat = node ? glm::inverse(node->global) * rbMat : rbMat;
	m_kinematicMotionState = node
		? std::unique_ptr<MotionState>(std::make_unique<KinematicMotionState>(node, m_offsetMat))
		: std::unique_ptr<MotionState>(std::make_unique<DefaultMotionState>(m_offsetMat));
	if (pmxRigidBody.op != Operation::Static) {
		if (node) {
			if (pmxRigidBody.op == Operation::Dynamic)
				m_activeMotionState = std::make_unique<DynamicMotionState>(node, m_offsetMat);
			else
				m_activeMotionState = std::make_unique<DynamicAndBoneMergeMotionState>(node, m_offsetMat);
		} else
			m_activeMotionState = std::make_unique<DefaultMotionState>(m_offsetMat);
	}
	btMotionState* motionState = m_activeMotionState ? m_activeMotionState.get() : m_kinematicMotionState.get();
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, m_shape.get(), localInertia);
	rbInfo.m_linearDamping = pmxRigidBody.translateDimmer;
	rbInfo.m_angularDamping = pmxRigidBody.rotateDimmer;
	rbInfo.m_restitution = pmxRigidBody.repulsion;
	rbInfo.m_friction = pmxRigidBody.friction;
	rbInfo.m_additionalDamping = true;
	rigidBody = std::make_unique<btRigidBody>(rbInfo);
	rigidBody->setUserPointer(this);
	rigidBody->setSleepingThresholds(0.01f, glm::radians(0.1f));
	rigidBody->setActivationState(DISABLE_DEACTIVATION);
	if (pmxRigidBody.op == Operation::Static)
		rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	m_rigidBodyType = pmxRigidBody.op;
	group = pmxRigidBody.group;
	groupMask = pmxRigidBody.collisionGroup;
	m_node = node;
	m_name = pmxRigidBody.name;
}

void RigidBody::ApplyActivation(const bool activation) const {
	if (m_rigidBodyType != Operation::Static) {
		if (activation) {
			rigidBody->setCollisionFlags(
				rigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
			rigidBody->setMotionState(m_activeMotionState.get());
		} else {
			rigidBody->setCollisionFlags(
				rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			rigidBody->setMotionState(m_kinematicMotionState.get());
		}
	} else
		rigidBody->setMotionState(m_kinematicMotionState.get());
}

void RigidBody::ResetTransform() const {
	if (m_activeMotionState)
		m_activeMotionState->Reset();
}

void RigidBody::Reset(const Physics* physics) const {
	if (const auto cache = physics->world->getPairCache()) {
		const auto dispatcher = physics->world->getDispatcher();
		cache->cleanProxyFromPairs(rigidBody->getBroadphaseHandle(), dispatcher);
	}
	rigidBody->setAngularVelocity(btVector3(0, 0, 0));
	rigidBody->setLinearVelocity(btVector3(0, 0, 0));
	rigidBody->clearForces();
}

void RigidBody::ReflectGlobalTransform() const {
	if (m_activeMotionState)
		m_activeMotionState->ReflectGlobalTransform();
	if (m_kinematicMotionState)
		m_kinematicMotionState->ReflectGlobalTransform();
}

void RigidBody::CalcLocalTransform() const {
	if (const auto node = m_node.lock()) {
		if (const auto parent = node->parent.lock()) {
			const auto local = glm::inverse(parent->global) * node->global;
			node->local = local;
		} else
			node->local = node->global;
	}
}

glm::mat4 RigidBody::CalcTransform() const {
	const btTransform transform = rigidBody->getCenterOfMassTransform();
	glm::mat4 mat;
	transform.getOpenGLMatrix(&mat[0][0]);
	return Util::InvZ(mat);
}

void Joint::Create(const PMXReader::PMXJoint& pmxJoint, const RigidBody* rigidBodyA, const RigidBody* rigidBodyB) {
	constraint = nullptr;
	btMatrix3x3 rotMat;
	rotMat.setEulerZYX(pmxJoint.rotate.x, pmxJoint.rotate.y, pmxJoint.rotate.z);
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(pmxJoint.translate.x, pmxJoint.translate.y, pmxJoint.translate.z));
	transform.setBasis(rotMat);
	btTransform invA = rigidBodyA->rigidBody->getWorldTransform().inverse();
	btTransform invB = rigidBodyB->rigidBody->getWorldTransform().inverse();
	invA = invA * transform;
	invB = invB * transform;
	auto jointConstraint = std::make_unique<btGeneric6DofSpringConstraint>(
		*rigidBodyA->rigidBody, *rigidBodyB->rigidBody,
		invA, invB, true);
	jointConstraint->setLinearLowerLimit(btVector3(pmxJoint.translateLowerLimit.x, pmxJoint.translateLowerLimit.y, pmxJoint.translateLowerLimit.z));
	jointConstraint->setLinearUpperLimit(btVector3(pmxJoint.translateUpperLimit.x, pmxJoint.translateUpperLimit.y, pmxJoint.translateUpperLimit.z));
	jointConstraint->setAngularLowerLimit(btVector3(pmxJoint.rotateLowerLimit.x, pmxJoint.rotateLowerLimit.y, pmxJoint.rotateLowerLimit.z));
	jointConstraint->setAngularUpperLimit(btVector3(pmxJoint.rotateUpperLimit.x, pmxJoint.rotateUpperLimit.y, pmxJoint.rotateUpperLimit.z));
	const float stiffness[6] = {
		pmxJoint.springTranslateFactor.x,
		pmxJoint.springTranslateFactor.y,
		pmxJoint.springTranslateFactor.z,
		pmxJoint.springRotateFactor.x,
		pmxJoint.springRotateFactor.y,
		pmxJoint.springRotateFactor.z,
	};
	for (int i = 0; i < 6; i++) {
		if (stiffness[i] != 0.0f) {
			jointConstraint->enableSpring(i, true);
			jointConstraint->setStiffness(i, stiffness[i]);
		}
	}
	constraint = std::move(jointConstraint);
}

Physics::~Physics() {
	if (world && m_groundRB)
		world->removeRigidBody(m_groundRB.get());
}

void Physics::Create() {
	m_broadPhase = std::make_unique<btDbvtBroadphase>();
	m_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
	m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfig.get());
	m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();
	world = std::make_unique<btDiscreteDynamicsWorld>(
		m_dispatcher.get(),
		m_broadPhase.get(),
		m_solver.get(),
		m_collisionConfig.get()
	);
	world->setGravity(btVector3(0, -9.8f * 10.0f, 0));
	m_groundShape = std::make_unique<btStaticPlaneShape>(btVector3(0, 1, 0), 0.0f);
	btTransform groundTransform;
	groundTransform.setIdentity();
	m_groundMS = std::make_unique<btDefaultMotionState>(groundTransform);
	btRigidBody::btRigidBodyConstructionInfo groundInfo(0, m_groundMS.get(), m_groundShape.get(), btVector3(0, 0, 0));
	m_groundRB = std::make_unique<btRigidBody>(groundInfo);
	world->addRigidBody(m_groundRB.get());
	auto filterCB = std::make_unique<OverlapFilterCallback>();
	filterCB->nonFilterProxy.push_back(m_groundRB->getBroadphaseProxy());
	world->getPairCache()->setOverlapFilterCallback(filterCB.get());
	m_filterCB = std::move(filterCB);
}

