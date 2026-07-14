#pragma once

#include "Core/Model/Model.h"

#include <span>
#include <vector>
#include <glm/glm.hpp>

namespace Chrivent {
	struct ViewerVertex {
		glm::vec3 position{};
		glm::vec3 normal{};
		glm::vec2 uv{};
		glm::vec3 previousPosition{};
	};

	struct ViewerIndexData {
		std::vector<char> bytes;
		size_t indexCount = 0;
		size_t elementSize = 0;
	};

	class ViewerGeometry {
	public:
		// 모델 geometry를 렌더러가 제공한 vertex 메모리에 직접 기록한다.
		static bool WriteVertices(const ModelGeometryData& geometryData, bool useUpdateData, std::span<ViewerVertex> destination);
		// PMX index element 크기에 맞춰 렌더러가 사용할 수 있는 index bytes를 만든다.
		static bool BuildIndexData(const ModelGeometryData& geometryData, ViewerIndexData& outIndexData);
	};
}
