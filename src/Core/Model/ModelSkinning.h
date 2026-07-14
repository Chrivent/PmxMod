#pragma once

#include "Core/Model/Model.h"

namespace Chrivent {
	// 본 변환과 모프를 적용해 렌더링용 버텍스를 계산한다.
	class ModelSkinning {
		Model& model;

		// 병렬 정점 갱신에 사용할 작업 범위를 구성한다.
		void SetupParallelUpdate() const;

	public:
		explicit ModelSkinning(Model& model) : model(model) {}

		std::size_t GetUpdateRangeCount() const { return model.geometryData.updateRanges.size(); }

		// 정점 수와 CPU 코어 수에 맞춰 스키닝 작업 범위와 선택적인 이전 위치를 준비한다.
		void PrepareUpdate(bool preservePreviousPositions) const;
		// 지정된 작업 범위의 스키닝 결과를 갱신한다.
		void UpdateRange(std::size_t rangeIndex) const;
	};
}
