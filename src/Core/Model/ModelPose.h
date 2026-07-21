#pragma once

#include "Core/Model/Model.h"

namespace Chrivent {
	// 본 계층의 전역 변환과 스키닝 행렬을 갱신한다.
	class ModelPose {
	public:
		// 물리 전후 단계에 맞는 노드 변환과 IK를 갱신한다.
		static void UpdateNodeAnimation(Model& model, bool afterPhysicsAnim);
		// 강체와 조인트를 현재 모델 포즈 기준으로 초기화한다.
		static void ResetPhysics(Model& model) { model.ResetPhysics(); }
		// 물리 시뮬레이션을 진행하고 강체 변환을 노드에 반영한다.
		static void UpdatePhysicsAnimation(Model& model, const float elapsed) { model.UpdatePhysics(elapsed); }
		// 현재 노드 전역 행렬을 스키닝용 최종 본 행렬로 변환한다.
		static void UpdateTransforms(Model& model);
	};
}
