#pragma once

#include "PMXFile.h"

#include <glm/mat4x4.hpp>

#include <vector>
#include <memory>
#include <cinttypes>

#include <btBulletDynamicsCommon.h>

class MMDPhysics;
class MMDModel;
class MMDNode;

class MMDMotionState : public btMotionState
{
public:
	virtual void Reset() = 0;
	virtual void ReflectGlobalTransform() = 0;
};

struct MMDFilterCallback final : btOverlapFilterCallback
{
	bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;

	std::vector<btBroadphaseProxy*> m_nonFilterProxy;
};

class DefaultMotionState final : public MMDMotionState
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

class DynamicMotionState final : public MMDMotionState
{
public:
	DynamicMotionState(MMDNode* node, const glm::mat4& offset, bool override = true);

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override;

	void Reset() override;
	void ReflectGlobalTransform() override;

private:
	MMDNode* m_node;
	glm::mat4	m_offset;
	glm::mat4	m_invOffset{};
	btTransform	m_transform;
	bool		m_override;
};

class DynamicAndBoneMergeMotionState final : public MMDMotionState
{
public:
	DynamicAndBoneMergeMotionState(MMDNode* node, const glm::mat4& offset, bool override = true);

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override;

	void Reset() override;
	void ReflectGlobalTransform() override;

private:
	MMDNode* m_node;
	glm::mat4	m_offset;
	glm::mat4	m_invOffset{};
	btTransform	m_transform;
	bool		m_override;
};

class KinematicMotionState final : public MMDMotionState
{
public:
	KinematicMotionState(MMDNode* node, const glm::mat4& offset);

	void getWorldTransform(btTransform& worldTransform) const override;
	void setWorldTransform(const btTransform& worldTransform) override;

	void Reset() override;
	void ReflectGlobalTransform() override;

private:
	MMDNode* m_node;
	glm::mat4	m_offset;
};

class MMDRigidBody
{
public:
	MMDRigidBody();

	bool Create(const PMXRigidbody& pmxRigidBody, const MMDModel* model, MMDNode* node);
	void Destroy();

	btRigidBody* GetRigidBody() const;

	void SetActivation(bool activation) const;
	void ResetTransform() const;
	void Reset(const MMDPhysics* physics) const;

	void ReflectGlobalTransform() const;
	void CalcLocalTransform() const;

	glm::mat4 GetTransform() const;

private:
	std::unique_ptr<btCollisionShape>	m_shape;
	std::unique_ptr<MMDMotionState>		m_activeMotionState;
	std::unique_ptr<MMDMotionState>		m_kinematicMotionState;
	std::unique_ptr<btRigidBody>		m_rigidBody;

public:
	Operation		m_rigidBodyType;
	uint16_t		m_group;
	uint16_t		m_groupMask;

	MMDNode*	m_node;
	glm::mat4	m_offsetMat;

	std::string					m_name;
};

class MMDJoint
{
public:
	bool CreateJoint(const PMXJoint& pmxJoint, const MMDRigidBody* rigidBodyA, const MMDRigidBody* rigidBodyB);
	void Destroy();

	btTypedConstraint* GetConstraint() const;

private:
	std::unique_ptr<btTypedConstraint>	m_constraint;
};

class MMDPhysics
{
public:
	MMDPhysics();
	~MMDPhysics();

	void Create();
	void Destroy();

	void Update(float time) const;

	void AddRigidBody(const MMDRigidBody* mmdRB) const;
	void RemoveRigidBody(const MMDRigidBody* mmdRB) const;
	void AddJoint(const MMDJoint* mmdJoint) const;
	void RemoveJoint(const MMDJoint* mmdJoint) const;

	btDiscreteDynamicsWorld* GetDynamicsWorld() const;

private:
	std::unique_ptr<btBroadphaseInterface>				m_broadPhase;
	std::unique_ptr<btDefaultCollisionConfiguration>	m_collisionConfig;
	std::unique_ptr<btCollisionDispatcher>				m_dispatcher;
	std::unique_ptr<btSequentialImpulseConstraintSolver>	m_solver;
	std::unique_ptr<btDiscreteDynamicsWorld>			m_world;
	std::unique_ptr<btCollisionShape>					m_groundShape;
	std::unique_ptr<btMotionState>						m_groundMS;
	std::unique_ptr<btRigidBody>						m_groundRB;
	std::unique_ptr<btOverlapFilterCallback>			m_filterCB;

public:
	double	m_fps;
	int		m_maxSubStepCount;
};

class MMDPhysicsManager
{
public:
	~MMDPhysicsManager();

	void Create();

	MMDPhysics* GetMMDPhysics() const;

	MMDRigidBody* AddRigidBody();
	MMDJoint* AddJoint();

private:
	std::unique_ptr<MMDPhysics>	m_mmdPhysics;

public:
	std::vector<std::unique_ptr<MMDRigidBody>>	m_rigidBodies;
	std::vector<std::unique_ptr<MMDJoint>>		m_joints;
};
