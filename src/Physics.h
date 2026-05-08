#pragma once

#include <memory>
#include <vector>
#include <btBulletDynamicsCommon.h>

#include "Reader.h"

struct Physics;
class Model;
struct Node;

class MotionState : public btMotionState {
public:
	/// 모션 상태를 초기 물리 변환으로 되돌린다.
	virtual void Reset() {}
	/// 물리 월드의 글로벌 변환을 연결된 본에 반영한다.
	virtual void ReflectGlobalTransform() {}
};

struct OverlapFilterCallback final : btOverlapFilterCallback {
	std::vector<btBroadphaseProxy*> m_nonFilterProxy;

	/// 두 broadphase 프록시가 충돌 후보가 될 수 있는지 필터링한다.
	bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;
};

class DefaultMotionState final : public MotionState {
public:
	explicit DefaultMotionState(const glm::mat4& transform);

	btTransform	m_initialTransform;
	btTransform	m_transform;

	/// Bullet에 현재 월드 변환을 전달한다.
	void getWorldTransform(btTransform& worldTransform) const override { worldTransform = m_transform; }
	/// Bullet에서 계산된 월드 변환을 저장한다.
	void setWorldTransform(const btTransform& worldTransform) override { m_transform = worldTransform; }
	/// 저장된 초기 변환으로 되돌린다.
	void Reset() override { m_transform = m_initialTransform; }
};

class DynamicMotionState : public MotionState {
public:
	DynamicMotionState(const std::shared_ptr<Node>& node, const glm::mat4& offset);

	std::weak_ptr<Node>	m_node;
	glm::mat4	m_offset;
	glm::mat4	m_invOffset = glm::mat4(1);
	btTransform	m_transform;

	/// Bullet에 현재 동적 강체 변환을 전달한다.
	void getWorldTransform(btTransform& worldTransform) const override { worldTransform = m_transform; }
	/// Bullet에서 계산된 동적 강체 변환을 저장한다.
	void setWorldTransform(const btTransform& worldTransform) override { m_transform = worldTransform; }
	/// 연결된 본의 현재 변환 기준으로 물리 변환을 재설정한다.
	void Reset() override;
	/// 물리 변환을 연결된 본의 글로벌 변환에 반영한다.
	void ReflectGlobalTransform() override;

protected:
	/// Bullet 글로벌 변환을 본에 반영하기 전에 파생 클래스가 보정한다.
	virtual void PostProcessBtGlobal(glm::mat4& btGlobal) const {}
};

class DynamicAndBoneMergeMotionState final : public DynamicMotionState {
public:
	using DynamicMotionState::DynamicMotionState;

protected:
	/// 물리 변환과 본 변환을 병합하기 위한 후처리를 수행한다.
	void PostProcessBtGlobal(glm::mat4& btGlobal) const override;
};

class KinematicMotionState final : public MotionState {
public:
	KinematicMotionState(const std::shared_ptr<Node>& node, const glm::mat4& offset);

	std::weak_ptr<Node>	m_node;
	glm::mat4	m_offset;

	/// 연결된 본 변환을 Bullet 월드 변환으로 변환한다.
	void getWorldTransform(btTransform& worldTransform) const override;
	/// 키네마틱 강체는 Bullet에서 쓰는 월드 변환 갱신을 무시한다.
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
	std::weak_ptr<Node>	m_node;
	glm::mat4	m_offsetMat = glm::mat4(1);
	std::string	m_name;

	/// PMX 강체 정보를 Bullet 강체와 모션 상태로 생성한다.
	void Create(const PMXReader::PMXRigidbody& pmxRigidBody, const Model* model, const std::shared_ptr<Node>& node);
	void ApplyActivation(bool activation) const;
	/// 강체 변환을 초기 위치로 재설정한다.
	void ResetTransform() const;
	/// 물리 월드에 등록된 강체 상태를 초기화한다.
	void Reset(const Physics* physics) const;
	/// 물리 계산 결과를 연결된 본 변환에 반영한다.
	void ReflectGlobalTransform() const;
	/// PMX 오프셋 기준의 로컬 변환을 계산한다.
	void CalcLocalTransform() const;
	glm::mat4 CalcTransform() const;
};

struct Joint {
	std::unique_ptr<btTypedConstraint>	m_constraint;

	/// PMX 조인트 정보를 두 강체 사이의 Bullet 제약으로 생성한다.
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

	/// Bullet 월드와 기본 물리 리소스를 생성한다.
	void Create();
};
