#pragma once

#include "Core/Model/Model.h"

namespace Chrivent {
	class Animation;

	// 모델 갱신 단계별 CPU 소요 시간을 보관한다.
	struct ModelUpdateTiming {
		double initializeMilliseconds = 0.0;
		double animationEvaluateMilliseconds = 0.0;
		double morphMilliseconds = 0.0;
		double beforePhysicsPoseMilliseconds = 0.0;
		double physicsMilliseconds = 0.0;
		double afterPhysicsPoseMilliseconds = 0.0;
		double transformMilliseconds = 0.0;
	};

	// 한 프레임의 모델 갱신에 필요한 입력과 선택 기능을 명시적으로 묶는다.
	struct ModelUpdateOptions {
		const Animation* animation = nullptr;
		float frame = 0.0f;
		float physicsElapsed = 0.0f;
		bool preservePreviousPositions = false;
		bool updatePhysics = true;
		ModelUpdateTiming* timing = nullptr;
	};

	// 애니메이션, 물리, 포즈와 스키닝 갱신 순서를 조정한다.
	class ModelUpdater {
	public:
		// 지정한 프레임의 포즈를 복원하고 물리 상태를 해당 시점에 맞춰 초기화한다.
		static void ResetPhysicsAtFrame(Model& model, const Animation& animation, float frame);
		// 지정한 애니메이션 시간으로 모델 상태를 평가하고 스키닝 작업 범위를 준비한다.
		static void Prepare(Model& model, const ModelUpdateOptions& options);
		// 현재 모델의 정점 갱신 범위를 기준으로 스키닝 작업 수를 계산한다.
		static std::size_t CalculateSkinningTaskCount(const Model& model);
		// 지정된 범위의 CPU 스키닝을 수행한다.
		static void UpdateSkinning(Model& model, std::size_t taskIndex);
	};
}
