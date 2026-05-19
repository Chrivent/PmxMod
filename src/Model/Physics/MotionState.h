#pragma once

#include <memory>
#include <glm/gtc/quaternion.hpp>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	struct Node;

	class MotionState : public btMotionState {
	public:
		~MotionState() override;

		// 모션 상태를 초기 물리 변환으로 되돌린다.
		virtual void Reset() {}
		// 물리 월드의 글로벌 변환을 연결된 본에 반영한다.
		virtual void ReflectGlobalTransform() {}
	};

	class DefaultMotionState final : public MotionState {
		btTransform	initialTransform;
		btTransform	transform;

	public:
		explicit DefaultMotionState(const glm::mat4& initialMatrix);
		~DefaultMotionState() override;

		// Bullet에 현재 월드 변환을 전달한다.
		void getWorldTransform(btTransform& worldTransform) const override { worldTransform = transform; }
		// Bullet에서 계산된 월드 변환을 저장한다.
		void setWorldTransform(const btTransform& worldTransform) override { transform = worldTransform; }
		// 저장된 초기 변환으로 되돌린다.
		void Reset() override { transform = initialTransform; }
	};

	class DynamicMotionState : public MotionState {
		glm::mat4	offset;
		glm::mat4	invOffset = glm::mat4(1);
		btTransform	transform;

	protected:
		std::weak_ptr<Node>	node;

		// Bullet 글로벌 변환을 본에 반영하기 전에 파생 클래스가 보정한다.
		virtual void PostProcessBtGlobal(glm::mat4& btGlobal) const {}

	public:
		DynamicMotionState(const std::shared_ptr<Node>& nodePtr, const glm::mat4& offsetMatrix);

		// Bullet에 현재 동적 강체 변환을 전달한다.
		void getWorldTransform(btTransform& worldTransform) const override { worldTransform = transform; }
		// Bullet에서 계산된 동적 강체 변환을 저장한다.
		void setWorldTransform(const btTransform& worldTransform) override { transform = worldTransform; }
		// 연결된 본의 현재 변환 기준으로 물리 변환을 재설정한다.
		void Reset() override;
		// 물리 변환을 연결된 본의 글로벌 변환에 반영한다.
		void ReflectGlobalTransform() override;
	};

	class DynamicAndBoneMergeMotionState final : public DynamicMotionState {
	protected:
		// 물리 변환과 본 변환을 병합하기 위한 후처리를 수행한다.
		void PostProcessBtGlobal(glm::mat4& btGlobal) const override;

	public:
		using DynamicMotionState::DynamicMotionState;
	};

	class KinematicMotionState final : public MotionState {
		std::weak_ptr<Node>	node;
		glm::mat4			offset;

	public:
		KinematicMotionState(const std::shared_ptr<Node>& nodePtr, const glm::mat4& offsetMatrix);

		// 연결된 본 변환을 Bullet 월드 변환으로 변환한다.
		void getWorldTransform(btTransform& worldTransform) const override;
		// 키네마틱 강체는 Bullet에서 쓰는 월드 변환 갱신을 무시한다.
		void setWorldTransform(const btTransform& worldTransform) override {}
	};
}
