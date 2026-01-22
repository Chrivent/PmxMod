#pragma once

#include <vector>
#include <btBulletDynamicsCommon.h>

#include "Reader.h"

struct Physics;
class Model;
struct Node;

class MotionState : public btMotionState {
public:
	virtual void Reset() = 0;
	virtual void ReflectGlobalTransform() = 0;
};

struct OverlapFilterCallback final : btOverlapFilterCallback {
	bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;

	std::vector<btBroadphaseProxy*> m_nonFilterProxy;
};

class DefaultMotionState final : public MotionState {
public:
	explicit DefaultMotionState(const glm::mat4& transform);

	void getWorldTransform(btTransform& worldTransform) const override { worldTransform = m_transform; }
	void setWorldTransform(const btTransform& worldTransform) override { m_transform = worldTransform; }

	void Reset() override { m_transform = m_initialTransform; }
	void ReflectGlobalTransform() override {}

private:
	btTransform	m_initialTransform;
	btTransform	m_transform;
};

class DynamicMotionState : public MotionState {
public:
	DynamicMotionState(Node* node, const glm::mat4& offset, bool overrideNode = true);

	void getWorldTransform(btTransform& worldTransform) const override { worldTransform = m_transform; }
	void setWorldTransform(const btTransform& worldTransform) override { m_transform = worldTransform; }

	void Reset() override;
	void ReflectGlobalTransform() override;

protected:
	virtual void PostProcessBtGlobal(glm::mat4& btGlobal) const {}

	Node* m_node;
	glm::mat4	m_offset;
	glm::mat4	m_invOffset{};
	btTransform	m_transform;
	bool		m_overrideNode;
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

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override {}

	void Reset() override {}
	void ReflectGlobalTransform() override {}

private:
	Node* m_node;
	glm::mat4	m_offset;
};

struct RigidBody {
	void Create(const PMXReader::PMXRigidbody& pmxRigidBody, const Model* model, Node* node);

	void SetActivation(bool activation) const;
	void ResetTransform() const;
	void Reset(const Physics* physics) const;

	void ReflectGlobalTransform() const;
	void CalcLocalTransform() const;

	glm::mat4 GetTransform() const;

	std::unique_ptr<btCollisionShape>	m_shape;
	std::unique_ptr<MotionState>		m_activeMotionState;
	std::unique_ptr<MotionState>		m_kinematicMotionState;
	std::unique_ptr<btRigidBody>		m_rigidBody;

	Operation		m_rigidBodyType = Operation::Static;
	uint16_t		m_group = 0;
	uint16_t		m_groupMask = 0;

	Node*	m_node = nullptr;
	glm::mat4	m_offsetMat = glm::mat4(1);

	std::string					m_name;
};

struct Joint {
	void CreateJoint(const PMXReader::PMXJoint& pmxJoint, const RigidBody* rigidBodyA, const RigidBody* rigidBodyB);

	std::unique_ptr<btTypedConstraint>	m_constraint;
};

struct Physics {
	~Physics();

	void Create();

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
};
