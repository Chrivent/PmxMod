#pragma once

#include "Core/Model/Model.h"

namespace Chrivent {
	class Animation;

	struct ModelUpdateTiming {
		double initializeMilliseconds = 0.0;
		double animationEvaluateMilliseconds = 0.0;
		double morphMilliseconds = 0.0;
		double beforePhysicsPoseMilliseconds = 0.0;
		double physicsMilliseconds = 0.0;
		double afterPhysicsPoseMilliseconds = 0.0;
		double transformMilliseconds = 0.0;
	};

	class ModelUpdater {
		Model& model;

	public:
		explicit ModelUpdater(Model& targetModel) : model(targetModel) {}

		// 지정한 애니메이션 시간으로 모델 상태를 평가하고 스키닝 작업 범위를 준비한다.
		void Prepare(const Animation* animation, float frame, float physicsElapsed, bool preservePreviousPositions,
			bool updatePhysics = true, ModelUpdateTiming* timing = nullptr) const;
		// 현재 모델의 정점 갱신 범위를 기준으로 스키닝 작업 수를 계산한다.
		std::size_t CalculateSkinningTaskCount() const;
		// 지정된 범위의 CPU 스키닝을 수행한다.
		void UpdateSkinning(std::size_t taskIndex) const;
	};
}
