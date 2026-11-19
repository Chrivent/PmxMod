#pragma once

#include <glm/glm.hpp>
#include <btBulletDynamicsCommon.h>

namespace Chrivent {
	class Node;

	// 모델 본과 Bullet 강체 사이의 변환 동기화 규약을 정의한다.
	class MotionState : public btMotionState {
	protected:
		float sceneScale;
		explicit MotionState(const float scale) : sceneScale(scale) {}
		// 본의 회전축은 유지하고 위치만 씬 단위로 바꾸어 Bullet에 전달한다.
		btTransform ToWorldTransform(const glm::mat4& modelMatrix) const;

	public:
		// 물리 변환을 초기 상태로 되돌린다.
		virtual void Reset() {}
		// 물리 변환을 연결된 본 변환에 반영한다.
		virtual void ReflectGlobalTransform() {}
	};

	// 연결된 본이 없는 강체의 초기 위치와 현재 물리 변환을 보관한다.
	class DefaultMotionState final : public MotionState {
		btTransform	initialTransform;
		btTransform	transform;

	public:
		DefaultMotionState(const glm::mat4& initialMatrix, float scale);

		void getWorldTransform(btTransform& worldTransform) const override { worldTransform = transform; }
		void setWorldTransform(const btTransform& worldTransform) override { transform = worldTransform; }
		
		// 기본 모션 상태의 변환을 생성 시점 값으로 되돌린다.
		void Reset() override { transform = initialTransform; }
	};

	// 물리 강체의 결과를 모델 본에 반영하는 동적 동기화를 구현한다.
	class DynamicMotionState : public MotionState {
		glm::mat4	offset;
		glm::mat4	invOffset = glm::mat4(1);
		btTransform	transform;

	protected:
		Node*	node = nullptr;

		// Bullet 글로벌 변환을 본에 반영하기 전에 파생 클래스별 후처리를 수행한다.
		virtual void PostProcessBtGlobal(glm::mat4&) const {}

	public:
		DynamicMotionState(Node* sourceNode, const glm::mat4& offsetMatrix, float scale);

		void getWorldTransform(btTransform& worldTransform) const override { worldTransform = transform; }
		void setWorldTransform(const btTransform& worldTransform) override { transform = worldTransform; }
		
		// 연결된 본의 현재 변환 기준으로 물리 변환을 재설정한다.
		void Reset() override;
		// 물리 변환을 연결된 본의 글로벌 변환에 반영한다.
		void ReflectGlobalTransform() override;
	};

	// 물리 결과와 본 애니메이션을 함께 반영하는 동기화를 구현한다.
	class DynamicAndBoneMergeMotionState final : public DynamicMotionState {
	protected:
		// 물리 변환과 본 변환을 병합하기 위한 후처리를 수행한다.
		void PostProcessBtGlobal(glm::mat4& btGlobal) const override;

	public:
		using DynamicMotionState::DynamicMotionState;
	};

	// 모델 본의 변환으로 키네마틱 강체를 구동한다.
	class KinematicMotionState final : public MotionState {
		Node*				node = nullptr;
		glm::mat4			offset;

	public:
		KinematicMotionState(Node* sourceNode, const glm::mat4& offsetMatrix, float scale);

		// 연결된 본 변환을 Bullet 월드 변환으로 변환한다.
		void getWorldTransform(btTransform& worldTransform) const override;
		void setWorldTransform(const btTransform&) override {}
	};
}
