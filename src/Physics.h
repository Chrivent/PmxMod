#pragma once

#include <vector>
#include <btBulletDynamicsCommon.h>

#include "Reader.h"

struct Physics;
class Model;
struct Node;

class MotionState : public btMotionState {
public:
	virtual void Reset() {}
	virtual void ReflectGlobalTransform() {}
};

struct OverlapFilterCallback final : btOverlapFilterCallback {
	std::vector<btBroadphaseProxy*> m_nonFilterProxy;

	bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;
};

class DefaultMotionState final : public MotionState {
public:
	explicit DefaultMotionState(const glm::mat4& transform);

	btTransform	m_initialTransform;
	btTransform	m_transform;

	void getWorldTransform(btTransform& worldTransform) const override { worldTransform = m_transform; }
	void setWorldTransform(const btTransform& worldTransform) override { m_transform = worldTransform; }
	void Reset() override { m_transform = m_initialTransform; }
};

class DynamicMotionState : public MotionState {
public:
	DynamicMotionState(Node* node, const glm::mat4& offset);

	Node*		m_node;
	glm::mat4	m_offset;
	glm::mat4	m_invOffset;
	btTransform	m_transform;

	void getWorldTransform(btTransform& worldTransform) const override { worldTransform = m_transform; }
	void setWorldTransform(const btTransform& worldTransform) override { m_transform = worldTransform; }
	void Reset() override;
	void ReflectGlobalTransform() override;

protected:
	virtual void PostProcessBtGlobal(glm::mat4& btGlobal) const {}
};

class DynamicAndBoneMergeMotionState final : public DynamicMotionState {
public:
	using DynamicMotionState::DynamicMotionState;

protected:
	void PostProcessBtGlobal(glm::mat4& btGlobal) const override;
};

class KinematicMotionState final : public MotionState {
public:
	KinematicMotionState(Node* node, const glm::mat4& offset);

	Node*		m_node;
	glm::mat4	m_offset;

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override {}
};

struct RigidBody {
	std::unique_ptr<btCollisionShape>	m_shape;
	std::unique_ptr<MotionState>		m_activeMotionState;
	std::unique_ptr<MotionState>		m_kinematicMotionState;
	std::unique_ptr<btRigidBody>		m_rigidBody;
	Operation	m_rigidBodyType = Operation::Static;
	uint16_t	m_group = 0;
	uint16_t	m_groupMask = 0;
	Node*		m_node = nullptr;
	glm::mat4	m_offsetMat = glm::mat4(1);
	std::string	m_name;

	void Create(const PMXReader::PMXRigidbody& pmxRigidBody, const Model* model, Node* node);
	void SetActivation(bool activation) const;
	void ResetTransform() const;
	void Reset(const Physics* physics) const;
	void ReflectGlobalTransform() const;
	void CalcLocalTransform() const;
	glm::mat4 GetTransform() const;
};

struct Joint {
	std::unique_ptr<btTypedConstraint>	m_constraint;

	void Create(const PMXReader::PMXJoint& pmxJoint, const RigidBody* rigidBodyA, const RigidBody* rigidBodyB);
};

struct Physics {
	~Physics();

	std::unique_ptr<btBroadphaseInterface>					m_broadPhase;
	std::unique_ptr<btDefaultCollisionConfiguration>		m_collisionConfig;
	std::unique_ptr<btCollisionDispatcher>					m_dispatcher;
	std::unique_ptr<btSequentialImpulseConstraintSolver>	m_solver;
	std::unique_ptr<btDiscreteDynamicsWorld>				m_world;
	std::unique_ptr<btCollisionShape>						m_groundShape;
	std::unique_ptr<btMotionState>							m_groundMS;
	std::unique_ptr<btRigidBody>							m_groundRB;
	std::unique_ptr<btOverlapFilterCallback>				m_filterCB;
	double	m_fps = 120.0f;
	int		m_maxSubStepCount = 10;

	void Create();
};
