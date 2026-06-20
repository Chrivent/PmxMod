#pragma once

#include "Model.h"

namespace Chrivent {
	class Animation;

	class ModelUpdater {
		Model& model;

	public:
		explicit ModelUpdater(Model& targetModel) : model(targetModel) {}

		// 지정한 애니메이션 시간으로 모델 상태를 평가하고 스키닝 작업 범위를 준비한다.
		void Prepare(const Animation* animation, float frame, float physicsElapsed, bool updatePhysics = true) const;
		// 준비된 CPU 스키닝 작업 범위 개수를 반환한다.
		std::size_t GetSkinningTaskCount() const;
		// 지정된 범위의 CPU 스키닝을 수행한다.
		void UpdateSkinning(std::size_t taskIndex) const;
	};
}
