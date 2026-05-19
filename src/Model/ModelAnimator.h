#pragma once

#include "Model.h"

namespace Chrivent {
	struct Animation;

	class ModelAnimator {
		Model& model;

	public:
		explicit ModelAnimator(Model& model) : model(model) {}

		// 애니메이션 평가에 필요한 기본 상태를 초기화한다.
		void InitializeAnimation() const;
		// 현재 애니메이션 상태를 기준 애니메이션으로 저장한다.
		void SaveBaseAnimation() const;
		// 저장된 기준 애니메이션 상태를 지운다.
		void ClearBaseAnimation() const;
		// 프레임 애니메이션 평가를 시작하기 전 상태를 준비한다.
		void BeginAnimation() const;
		// 현재 모프 가중치를 반영해 모프 애니메이션을 갱신한다.
		void UpdateMorphAnimation() const;
		// 지정한 애니메이션 프레임과 물리 시간으로 모든 애니메이션 단계를 갱신하고, 필요하면 물리 단계를 건너뛴다.
		void UpdateAllAnimation(const Animation* anim, float frame, float physicsElapsed, bool updatePhysics = true) const;
		// 물리 상태를 지정한 애니메이션 시간에 맞춰 동기화한다.
		void SyncPhysics(const Animation& anim, float frame) const;
	};
}
