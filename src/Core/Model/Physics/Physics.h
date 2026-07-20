#pragma once

#include "Core/Model/Physics/Joint.h"
#include "Core/Model/Physics/RigidBody.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <vector>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	class Node;

	// Bullet 물리 월드와 등록된 강체 및 조인트를 관리한다.
	class Physics {
		// 바닥 프록시는 충돌 그룹과 무관하게 통과시키는 Bullet broadphase 필터다.
		class OverlapFilterCallback final : public btOverlapFilterCallback {
			btBroadphaseProxy* unfilteredProxy = nullptr;

		public:
			void SetUnfilteredProxy(btBroadphaseProxy* proxy) { unfilteredProxy = proxy; }

			// 두 broadphase 프록시가 충돌 후보가 될 수 있는지 필터링한다.
			bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override;
		};

		static constexpr float simulationFps = 120.0f;
		static constexpr int maxSubStepCount = 10;

		std::unique_ptr<btBroadphaseInterface>					broadPhase;
		std::unique_ptr<btDefaultCollisionConfiguration>		collisionConfig;
		std::unique_ptr<btCollisionDispatcher>					dispatcher;
		std::unique_ptr<btSequentialImpulseConstraintSolver>	solver;
		std::unique_ptr<btDiscreteDynamicsWorld>				world;
		std::unique_ptr<btCollisionShape>						groundShape;
		std::unique_ptr<btMotionState>							groundMotionState;
		std::unique_ptr<btRigidBody>							groundRigidBody;
		std::unique_ptr<OverlapFilterCallback>					filterCallback;

		// Bullet 월드와 기본 물리 리소스를 생성한다.
		void Create();

	public:
		Physics();
		~Physics();

		// 강체를 충돌 그룹 설정과 함께 물리 월드에 등록한다.
		void AddRigidBody(btRigidBody& rigidBody, uint16_t group, uint16_t groupMask) const;
		// 물리 월드에서 강체를 제거한다.
		void RemoveRigidBody(btRigidBody& rigidBody) const;
		// 조인트 제약 조건을 물리 월드에 등록한다.
		void AddConstraint(btTypedConstraint& constraint) const;
		// 물리 월드에서 조인트 제약 조건을 제거한다.
		void RemoveConstraint(btTypedConstraint& constraint) const;
		// 지정한 경과 시간만큼 물리 시뮬레이션을 진행한다.
		void Step(float elapsed) const;
		// 강체에 남아 있는 broadphase 충돌 쌍을 제거한다.
		void CleanCollisionPairs(btRigidBody& rigidBody) const;
	};

	// 모델의 Bullet 물리 월드, 강체와 조인트를 함께 소유하고 등록 수명을 관리한다.
	class ModelPhysicsData {
		std::unique_ptr<Physics> physics;
		std::vector<std::unique_ptr<RigidBody>> rigidBodies;
		std::vector<std::unique_ptr<Joint>> joints;

		// 강체와 조인트 정의가 Bullet 생성 조건과 모델 참조 범위를 만족하는지 검증한다.
		static std::expected<void, PhysicsError> ValidateDefinitions(
			const std::vector<RigidBodyDefinition>& rigidBodyDefinitions,
			const std::vector<JointDefinition>& jointDefinitions, std::size_t nodeCount);
		// 강체 변환을 본에 반영하고 루트 노드의 글로벌 변환을 갱신한다.
		void ReflectTransforms(const std::vector<std::shared_ptr<Node>>& nodes) const;

	public:
		~ModelPhysicsData();

		bool IsInitialized() const { return physics != nullptr; }

		// 런타임 정의와 모델 노드로 물리 월드, 강체와 조인트를 구성한다.
		std::expected<void, PhysicsError> Initialize(const std::vector<RigidBodyDefinition>& rigidBodyDefinitions,
			const std::vector<JointDefinition>& jointDefinitions, const std::vector<std::shared_ptr<Node>>& nodes);
		// 현재 모델 포즈 기준으로 강체 상태와 충돌 쌍을 초기화한다.
		void ResetSimulation(const std::vector<std::shared_ptr<Node>>& nodes) const;
		// 지정한 경과 시간만큼 물리를 진행하고 강체 변환을 본에 반영한다.
		void UpdateSimulation(float elapsed, const std::vector<std::shared_ptr<Node>>& nodes) const;
		// 물리 월드에서 조인트와 강체를 제거하고 소유 리소스를 해제한다.
		void Reset();
	};
}
