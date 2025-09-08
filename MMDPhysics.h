#pragma once

#include "PMXFile.h"
#include "Time.h"

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <vector>
#include <memory>
#include <cinttypes>

#include <btBulletDynamicsCommon.h>

// Bullet Types
class btRigidBody;
class btCollisionShape;
class btTypedConstraint;
class btDiscreteDynamicsWorld;
class btBroadphaseInterface;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btSequentialImpulseConstraintSolver;
class btMotionState;
struct btOverlapFilterCallback;

namespace saba
{
	class MMDPhysics;
	class MMDModel;
	class MMDNode;

	class MMDMotionState : public btMotionState
	{
	public:
		virtual void Reset() = 0;
		virtual void ReflectGlobalTransform() = 0;
	};

	namespace
	{
		glm::mat4 InvZ(const glm::mat4& m)
		{
			const glm::mat4 invZ = glm::scale(glm::mat4(1), glm::vec3(1, 1, -1));
			return invZ * m * invZ;
		}
	}

	struct MMDFilterCallback : public btOverlapFilterCallback
	{
		bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;

		std::vector<btBroadphaseProxy*> m_nonFilterProxy;
	};

	class MMDRigidBody
	{
	public:
		MMDRigidBody();
		~MMDRigidBody();

		bool Create(const PMXRigidbody& pmxRigidBody, MMDModel* model, MMDNode* node);
		void Destroy();

		btRigidBody* GetRigidBody() const;

		void SetActivation(bool activation);
		void ResetTransform();
		void Reset(MMDPhysics* physics);

		void ReflectGlobalTransform();
		void CalcLocalTransform();

		glm::mat4 GetTransform();

	private:
		enum class RigidBodyType
		{
			Kinematic,
			Dynamic,
			Aligned,
		};

	private:
		std::unique_ptr<btCollisionShape>	m_shape;
		std::unique_ptr<MMDMotionState>		m_activeMotionState;
		std::unique_ptr<MMDMotionState>		m_kinematicMotionState;
		std::unique_ptr<btRigidBody>		m_rigidBody;

	public:
		RigidBodyType	m_rigidBodyType;
		uint16_t		m_group;
		uint16_t		m_groupMask;

		MMDNode*	m_node;
		glm::mat4	m_offsetMat;

		std::string					m_name;
	};

	class MMDJoint
	{
	public:
		MMDJoint();
		~MMDJoint();

		bool CreateJoint(const PMXJoint& pmxJoint, MMDRigidBody* rigidBodyA, MMDRigidBody* rigidBodyB);
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

		bool Create();
		void Destroy();

		void Update(float time);

		void AddRigidBody(MMDRigidBody* mmdRB);
		void RemoveRigidBody(MMDRigidBody* mmdRB);
		void AddJoint(MMDJoint* mmdJoint);
		void RemoveJoint(MMDJoint* mmdJoint);

		btDiscreteDynamicsWorld* GetDynamicsWorld() const;

	private:
		std::unique_ptr<btBroadphaseInterface>				m_broadphase;
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

		bool Create();

		MMDPhysics* GetMMDPhysics();

		MMDRigidBody* AddRigidBody();
		MMDJoint* AddJoint();

	private:
		std::unique_ptr<MMDPhysics>	m_mmdPhysics;

	public:
		std::vector<std::unique_ptr<MMDRigidBody>>	m_rigidBodys;
		std::vector<std::unique_ptr<MMDJoint>>		m_joints;
	};
}

