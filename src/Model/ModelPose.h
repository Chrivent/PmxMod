#pragma once

#include "Model.h"

namespace Chrivent {
	class ModelPose {
		Model& model;

	public:
		explicit ModelPose(Model& model) : model(model) {}

		// 물리 전후 단계에 맞는 노드 변환과 IK를 갱신한다.
		void UpdateNodeAnimation(bool afterPhysicsAnim) const;
		// 강체와 조인트를 현재 모델 포즈 기준으로 초기화한다.
		void ResetPhysics() const;
		// 물리 시뮬레이션을 진행하고 강체 변환을 노드에 반영한다.
		void UpdatePhysicsAnimation(float elapsed) const;
		// 현재 포즈를 기준으로 최종 스키닝 정점 버퍼를 갱신한다.
		void Update() const;
	};
}
