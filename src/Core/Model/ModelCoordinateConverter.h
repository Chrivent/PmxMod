#pragma once

#include <glm/gtc/matrix_transform.hpp>

namespace Chrivent {
	// 모델 행렬을 Z축 방향이 반대인 좌표계 사이에서 변환한다.
	class ModelCoordinateConverter {
	public:
		// 3x3 행렬의 입력과 출력 Z축을 함께 반전한다.
		static glm::mat3 ConvertZAxis(const glm::mat3& matrix) {
			constexpr glm::mat3 invertedZ(
				1, 0, 0,
				0, 1, 0,
				0, 0, -1);
			return invertedZ * matrix * invertedZ;
		}
		// 행렬의 입력과 출력 Z축을 함께 반전한다.
		static glm::mat4 ConvertZAxis(const glm::mat4& matrix) {
			const glm::mat4 invertedZ = glm::scale(glm::mat4(1), glm::vec3(1, 1, -1));
			return invertedZ * matrix * invertedZ;
		}
	};
}
