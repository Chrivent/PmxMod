#pragma once

#include "../Model/Model.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Chrivent {
	struct ViewerVertex {
		glm::vec3 position{};
		glm::vec3 normal{};
		glm::vec2 uv{};
	};

	struct ViewerIndexData {
		std::vector<char> bytes;
		size_t indexCount = 0;
		size_t elementSize = 0;
	};

	class ViewerGeometry {
	public:
		// 모델 geometry를 렌더러 공통 vertex 배열로 변환한다.
		static std::vector<ViewerVertex> BuildVertices(const ModelGeometryData& geometryData, bool useUpdateData);
		// PMX index element 크기에 맞춰 렌더러가 사용할 수 있는 index bytes를 만든다.
		static bool BuildIndexData(const ModelGeometryData& geometryData, ViewerIndexData& outIndexData);
	};
}
