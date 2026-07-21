#pragma once

namespace Chrivent {
	class Model;

	// 본 계층의 전역 변환과 스키닝 행렬을 갱신한다.
	class ModelPose {
	public:
		// 물리 전후 단계에 맞는 노드 변환과 IK를 갱신한다.
		static void UpdateNodeAnimation(Model& model, bool afterPhysicsAnim);
		// 현재 노드 전역 행렬을 스키닝용 최종 본 행렬로 변환한다.
		static void UpdateTransforms(Model& model);
	};
}
