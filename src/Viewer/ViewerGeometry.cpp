#include "ViewerGeometry.h"

namespace Chrivent {
	bool ViewerGeometry::BuildIndexData(const ModelGeometryData& geometryData, ViewerIndexData& outIndexData) {
		outIndexData = {};
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
