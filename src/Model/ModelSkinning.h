#pragma once

#include "Model.h"

namespace Chrivent::ModelSkinning {
	struct SkinningContext {
		const std::vector<glm::vec3>& positions;
		const std::vector<glm::vec3>& normals;
		const std::vector<glm::vec2>& uvs;
		const std::vector<Vertex>& vertexBoneInfos;
		const std::vector<glm::mat4>& transforms;
		const std::vector<glm::vec3>& morphPositions;
		const std::vector<glm::vec4>& morphUVs;
		std::vector<glm::vec3>& updatePositions;
		std::vector<glm::vec3>& updateNormals;
		std::vector<glm::vec2>& updateUVs;
		const std::vector<std::shared_ptr<Node>>& nodes;
	};

	// 지정된 버텍스 범위의 스키닝 결과를 갱신한다.
	void UpdateSkinning(const SkinningContext& context, const UpdateRange& range);
}
