#pragma once

#include "Model.h"

namespace Chrivent {
	class ModelSkinning {
		Model& model;

		// 병렬 정점 갱신에 사용할 작업 범위를 구성한다.
		void SetupParallelUpdate() const;
		// 지정된 정점 범위의 스키닝 결과를 갱신한다.
		void UpdateVertices(const UpdateRange& range) const;

	public:
		explicit ModelSkinning(Model& model) : model(model) {}

		// 현재 포즈를 기준으로 최종 스키닝 정점 버퍼를 갱신한다.
		void Update() const;
	};
}
