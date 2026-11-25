#include "Viewer/Geometry/ViewerGeometry.h"

namespace Chrivent {
	bool ViewerGeometry::ValidateIndexData(const ModelGeometryData& geometryData) {
		const size_t elementSize = geometryData.indexElementSize;
		if (geometryData.indexCount == 0 || (elementSize != 1 && elementSize != 2 && elementSize != 4))
			return false;
		return geometryData.indexCount <= geometryData.indices.size() / elementSize;
	}

	bool ViewerGeometry::WriteVertices(const ModelGeometryData& geometryData, const bool useUpdateData, const std::span<ViewerVertex> destination) {
		const auto& positions = useUpdateData && geometryData.updatePositions.size() == geometryData.positions.size()
			? geometryData.updatePositions
			: geometryData.positions;
		const auto& normals = useUpdateData && geometryData.updateNormals.size() == geometryData.normals.size()
			? geometryData.updateNormals
			: geometryData.normals;
		const auto& uvs = useUpdateData && geometryData.updateUVs.size() == geometryData.uvs.size()
			? geometryData.updateUVs
			: geometryData.uvs;
		const auto& previousPositions = useUpdateData && geometryData.previousPositions.size() == geometryData.positions.size()
			? geometryData.previousPositions
			: positions;
		if (destination.size() < positions.size())
			return false;
		const auto& additionalUv = useUpdateData && !geometryData.updateAdditionalUVs[0].empty()
			? geometryData.updateAdditionalUVs[0] : geometryData.additionalUVs[0];
		for (size_t index = 0; index < positions.size(); index++) {
			auto& [position, normal, uv, previousPosition, edgeScale, subTextureUv] = destination[index];
			position = positions[index];
			if (index < normals.size())
				normal = normals[index];
			if (index < uvs.size())
				uv = uvs[index];
			previousPosition = previousPositions[index];
			edgeScale = index < geometryData.edgeScales.size() ? geometryData.edgeScales[index] : 1.0f;
			subTextureUv = index < additionalUv.size() ? glm::vec2(additionalUv[index]) : glm::vec2(0);
		}
		return true;
	}

	bool ViewerGeometry::BuildIndexData(const ModelGeometryData& geometryData, ViewerIndexData& outIndexData) {
		outIndexData = {};
		if (!ValidateIndexData(geometryData))
			return false;
		outIndexData.indexCount = geometryData.indexCount;
		if (geometryData.indexElementSize == 1) {
			std::vector<uint16_t> convertedIndices(geometryData.indexCount);
			for (size_t index = 0; index < geometryData.indexCount; index++)
				convertedIndices[index] = static_cast<unsigned char>(geometryData.indices[index]);
			const size_t byteSize = sizeof(uint16_t) * convertedIndices.size();
			const auto* bytes = reinterpret_cast<const char*>(convertedIndices.data());
			outIndexData.bytes.assign(bytes, bytes + byteSize);
			outIndexData.elementSize = sizeof(uint16_t);
			return true;
		}
		if (geometryData.indexElementSize == 2) {
			outIndexData.bytes = geometryData.indices;
			outIndexData.elementSize = sizeof(uint16_t);
			return true;
		}
		if (geometryData.indexElementSize == 4) {
			outIndexData.bytes = geometryData.indices;
			outIndexData.elementSize = sizeof(uint32_t);
			return true;
		}
		return false;
	}
}
