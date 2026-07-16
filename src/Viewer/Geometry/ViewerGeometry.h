#pragma once

#include "Core/Model/Model.h"

#include <span>
#include <vector>
#include <glm/glm.hpp>

namespace Chrivent {
	// 모든 렌더링 API가 공유하는 변형 완료 버텍스 형식을 정의한다.
	struct ViewerVertex {
		glm::vec3 position{};
		glm::vec3 normal{};
		glm::vec2 uv{};
		glm::vec3 previousPosition{};
	};

	// 렌더러에 업로드할 인덱스 바이트와 요소 크기를 보관한다.
	struct ViewerIndexData {
		std::vector<char> bytes;
		size_t indexCount = 0;
		size_t elementSize = 0;
	};

	// 모델 형상을 공통 렌더링 버텍스와 인덱스 형식으로 변환한다.
	class ViewerGeometry {
	public:
		// 모델 index 정보가 지원 형식이고 원본 byte 범위 안에 있는지 검증한다.
		static bool ValidateIndexData(const ModelGeometryData& geometryData);
		// 모델 geometry를 렌더러가 제공한 vertex 메모리에 직접 기록한다.
		static bool WriteVertices(const ModelGeometryData& geometryData, bool useUpdateData, std::span<ViewerVertex> destination);
		// PMX index element 크기에 맞춰 렌더러가 사용할 수 있는 index bytes를 만든다.
		static bool BuildIndexData(const ModelGeometryData& geometryData, ViewerIndexData& outIndexData);
	};
}
