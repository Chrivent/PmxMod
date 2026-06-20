#pragma once

#include "../Core/Model/Model.h"

#include <vector>
#include <glm/glm.hpp>

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
		// 모델 geometry를 렌더러가 제공한 vertex 메모리에 직접 기록한다.
		template <typename Vertex>
		static bool WriteVertices(const ModelGeometryData& geometryData, bool useUpdateData, Vertex* destination,
			const size_t destinationCount) {
			const auto& positions = useUpdateData && geometryData.updatePositions.size() == geometryData.positions.size()
				? geometryData.updatePositions
				: geometryData.positions;
			const auto& normals = useUpdateData && geometryData.updateNormals.size() == geometryData.normals.size()
				? geometryData.updateNormals
				: geometryData.normals;
			const auto& uvs = useUpdateData && geometryData.updateUVs.size() == geometryData.uvs.size()
				? geometryData.updateUVs
				: geometryData.uvs;
			if (destination == nullptr || destinationCount < positions.size())
				return false;
			for (size_t index = 0; index < positions.size(); index++) {
				auto& [position, normal, uv] = destination[index];
				position = positions[index];
				if (index < normals.size())
					normal = normals[index];
				if (index < uvs.size())
					uv = uvs[index];
			}
			return true;
		}
		// PMX index element 크기에 맞춰 렌더러가 사용할 수 있는 index bytes를 만든다.
		static bool BuildIndexData(const ModelGeometryData& geometryData, ViewerIndexData& outIndexData);
	};
}
