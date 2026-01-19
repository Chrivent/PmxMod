#pragma once

#include <vector>
#include <btBulletDynamicsCommon.h>

#include "Reader.h"

struct Physics;
class Model;
struct Node;

class MotionState : public btMotionState
{
public:
	virtual void Reset() = 0;
	virtual void ReflectGlobalTransform() = 0;
};

struct OverlapFilterCallback final : btOverlapFilterCallback
{
	bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;

	std::vector<btBroadphaseProxy*> m_nonFilterProxy;
};

class DefaultMotionState final : public MotionState
{
public:
	explicit DefaultMotionState(const glm::mat4& transform);

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override;

	void Reset() override;
	void ReflectGlobalTransform() override;

private:
	btTransform	m_initialTransform;
	btTransform	m_transform;
};

class DynamicMotionState final : public MotionState
{
public:
	DynamicMotionState(Node* node, const glm::mat4& offset, bool override = true);

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override;

	void Reset() override;
	void ReflectGlobalTransform() override;

private:
	Node* m_node;
	glm::mat4	m_offset;
	glm::mat4	m_invOffset{};
	btTransform	m_transform;
	bool		m_override;
};

class DynamicAndBoneMergeMotionState final : public MotionState
{
public:
	DynamicAndBoneMergeMotionState(Node* node, const glm::mat4& offset, bool override = true);

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override;

	void Reset() override;
	void ReflectGlobalTransform() override;

private:
	Node* m_node;
	glm::mat4	m_offset;
	glm::mat4	m_invOffset{};
	btTransform	m_transform;
	bool		m_override;
};

class KinematicMotionState final : public MotionState
{
public:
	KinematicMotionState(Node* node, const glm::mat4& offset);

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override;

	void Reset() override;
	void ReflectGlobalTransform() override;

private:
	Node* m_node;
	glm::mat4	m_offset;
};

struct RigidBody
{
	RigidBody();

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

	Operation		m_rigidBodyType;
	uint16_t		m_group;
	uint16_t		m_groupMask;

	Node*	m_node;
	glm::mat4	m_offsetMat;

	std::string					m_name;
};

struct Joint
{
	void CreateJoint(const PMXReader::PMXJoint& pmxJoint, const RigidBody* rigidBodyA, const RigidBody* rigidBodyB);

	std::unique_ptr<btTypedConstraint>	m_constraint;
};

struct Physics
{
	Physics();
	~Physics();

	void Create();
	void Update(float time) const;

	void AddRigidBody(const RigidBody* rb) const;
	void RemoveRigidBody(const RigidBody* rb) const;
	void AddJoint(const Joint* joint) const;
	void RemoveJoint(const Joint* joint) const;

	std::unique_ptr<btBroadphaseInterface>					m_broadPhase;
	std::unique_ptr<btDefaultCollisionConfiguration>		m_collisionConfig;
	std::unique_ptr<btCollisionDispatcher>					m_dispatcher;
	std::unique_ptr<btSequentialImpulseConstraintSolver>	m_solver;
	std::unique_ptr<btDiscreteDynamicsWorld>				m_world;
	std::unique_ptr<btCollisionShape>						m_groundShape;
	std::unique_ptr<btMotionState>							m_groundMS;
	std::unique_ptr<btRigidBody>							m_groundRB;
	std::unique_ptr<btOverlapFilterCallback>				m_filterCB;

	double	m_fps;
	int		m_maxSubStepCount;
};
