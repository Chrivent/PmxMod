#include "Physics.h"

#include "../Model.h"
#include "../../Util.h"

namespace Chrivent {
	MotionState::~MotionState() = default;

	bool OverlapFilterCallback::needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const {
		const auto endIt = nonFilterProxy.end();
		if (std::ranges::find(nonFilterProxy, proxy0) != endIt || std::ranges::find(nonFilterProxy, proxy1) != endIt)
			return true;
		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask) != 0;
		return collides;
	}

	DefaultMotionState::DefaultMotionState(const glm::mat4& initialMatrix) {
		glm::mat4 trans = Util::InvZ(initialMatrix);
		transform.setFromOpenGLMatrix(&trans[0][0]);
		initialTransform = transform;
	}

	DefaultMotionState::~DefaultMotionState() = default;

	DynamicMotionState::DynamicMotionState(const std::shared_ptr<Node>& nodePtr, const glm::mat4& offsetMatrix)
		: offset(offsetMatrix), node(nodePtr) {
		invOffset = glm::inverse(offsetMatrix);
		DynamicMotionState::Reset();
	}

	void DynamicMotionState::Reset() {
		const auto nodePtr = node.lock();
		if (!nodePtr)
			return;
		glm::mat4 global = Util::InvZ(nodePtr->global * offset);
		transform.setFromOpenGLMatrix(&global[0][0]);
	}

	void DynamicMotionState::ReflectGlobalTransform() {
		const auto nodePtr = node.lock();
		if (!nodePtr)
			return;
		glm::mat4 worldTransformMat;
		transform.getOpenGLMatrix(&worldTransformMat[0][0]);
		glm::mat4 btGlobal = Util::InvZ(worldTransformMat) * invOffset;
		PostProcessBtGlobal(btGlobal);
		nodePtr->global = btGlobal;
		nodePtr->UpdateChildTransform();
	}

	void DynamicAndBoneMergeMotionState::PostProcessBtGlobal(glm::mat4& btGlobal) const {
		if (const auto nodePtr = node.lock())
			btGlobal[3] = nodePtr->global[3];
	}

	KinematicMotionState::KinematicMotionState(const std::shared_ptr<Node>& nodePtr, const glm::mat4& offsetMatrix)
		: node(nodePtr), offset(offsetMatrix) {}

	void KinematicMotionState::getWorldTransform(btTransform& worldTransform) const {
		const auto nodePtr = node.lock();
		if (!nodePtr)
			return;
		glm::mat4 global = Util::InvZ(nodePtr->global * offset);
		worldTransform.setFromOpenGLMatrix(&global[0][0]);
	}

	void RigidBody::Create(const PmxParser::PmxRigidbody& pmxRigidBody, const Model* model, const std::shared_ptr<Node>& nodePtr) {
		switch (pmxRigidBody.shape) {
			case Shape::Sphere:
				shape = std::make_unique<btSphereShape>(pmxRigidBody.shapeSize.x);
				break;
			case Shape::Box:
				shape = std::make_unique<btBoxShape>(btVector3(
					pmxRigidBody.shapeSize.x,
					pmxRigidBody.shapeSize.y,
					pmxRigidBody.shapeSize.z
				));
				break;
			case Shape::Capsule:
				shape = std::make_unique<btCapsuleShape>(
					pmxRigidBody.shapeSize.x,
					pmxRigidBody.shapeSize.y
				);
				break;
		}
		btScalar mass(0.0f);
		btVector3 localInertia(0, 0, 0);
		if (pmxRigidBody.op != Operation::Static)
			mass = pmxRigidBody.mass;
		if (mass > 0.0f)
			shape->calculateLocalInertia(mass, localInertia);
		const auto rx = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.x, glm::vec3(1, 0, 0));
		const auto ry = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.y, glm::vec3(0, 1, 0));
		const auto rz = glm::rotate(glm::mat4(1), pmxRigidBody.rotate.z, glm::vec3(0, 0, 1));
		const glm::mat4 rotMat = ry * rx * rz;
		const glm::mat4 translateMat = glm::translate(glm::mat4(1), pmxRigidBody.translate);
		const glm::mat4 rbMat = Util::InvZ(translateMat * rotMat);
		offsetMat = nodePtr ? glm::inverse(nodePtr->global) * rbMat : rbMat;
		kinematicMotionState = nodePtr
		? std::unique_ptr<MotionState>(std::make_unique<KinematicMotionState>(nodePtr, offsetMat))
		: std::unique_ptr<MotionState>(std::make_unique<DefaultMotionState>(offsetMat));
		if (pmxRigidBody.op != Operation::Static) {
			if (nodePtr) {
				if (pmxRigidBody.op == Operation::Dynamic)
					activeMotionState = std::make_unique<DynamicMotionState>(nodePtr, offsetMat);
				else
					activeMotionState = std::make_unique<DynamicAndBoneMergeMotionState>(nodePtr, offsetMat);
			} else
				activeMotionState = std::make_unique<DefaultMotionState>(offsetMat);
		}
		btMotionState* motionState = activeMotionState ? activeMotionState.get() : kinematicMotionState.get();
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape.get(), localInertia);
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
		rigidBodyType = pmxRigidBody.op;
		group = pmxRigidBody.group;
		groupMask = pmxRigidBody.collisionGroup;
		node = nodePtr;
		name = pmxRigidBody.name;
	}

	void RigidBody::ApplyActivation(const bool activation) const {
		if (rigidBodyType != Operation::Static) {
			if (activation) {
				rigidBody->setCollisionFlags(
					rigidBody->getCollisionFlags() & ~btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidBody->setMotionState(activeMotionState.get());
			} else {
				rigidBody->setCollisionFlags(
					rigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
				rigidBody->setMotionState(kinematicMotionState.get());
			}
		} else
			rigidBody->setMotionState(kinematicMotionState.get());
	}

	void RigidBody::ResetTransform() const {
		if (activeMotionState)
			activeMotionState->Reset();
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
		if (activeMotionState)
			activeMotionState->ReflectGlobalTransform();
		if (kinematicMotionState)
			kinematicMotionState->ReflectGlobalTransform();
	}

	void RigidBody::CalcLocalTransform() const {
		if (const auto nodePtr = node.lock()) {
			if (const auto parent = nodePtr->parent.lock()) {
				const auto local = glm::inverse(parent->global) * nodePtr->global;
				nodePtr->local = local;
			} else
				nodePtr->local = nodePtr->global;
		}
	}

	glm::mat4 RigidBody::CalcTransform() const {
		const btTransform transform = rigidBody->getCenterOfMassTransform();
		glm::mat4 mat;
		transform.getOpenGLMatrix(&mat[0][0]);
		return Util::InvZ(mat);
	}

	void Joint::Create(const PmxParser::PmxJoint& pmxJoint, const RigidBody* rigidBodyA, const RigidBody* rigidBodyB) {
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
