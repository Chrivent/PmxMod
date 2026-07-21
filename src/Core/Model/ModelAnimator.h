#pragma once

namespace Chrivent {
	class Animation;
	class Model;

	// 모델 애니메이션을 평가하고 본과 모프 상태에 반영한다.
	class ModelAnimator {
		// 현재 애니메이션 상태를 기준 애니메이션으로 저장한다.
		static void SaveBaseAnimation(const Model& model);
		// 저장된 기준 애니메이션 상태를 지운다.
		static void ClearBaseAnimation(const Model& model);

	public:
		// 애니메이션 평가에 필요한 기본 상태를 초기화한다.
		static void InitializeAnimation(Model& model);
		// 프레임 애니메이션 평가를 시작하기 전 상태를 준비한다.
		static void BeginAnimation(Model& model);
		// 물리 상태를 지정한 애니메이션 시간에 맞춰 동기화한다.
		static void SyncPhysics(Model& model, const Animation& animation, float frame);
	};
}
