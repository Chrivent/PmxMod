#pragma once

#include <glm/glm.hpp>

namespace Chrivent {
	class ViewerMatrix {
	public:
		// OpenGL 스타일 clip space를 DirectX depth range로 변환하는 행렬을 반환한다.
		static const glm::mat4& DirectXClipMatrix();
		// GL/DX와 같은 화면 좌표 및 깊이 범위로 맞추는 Vulkan clip 보정 행렬을 반환한다.
		static const glm::mat4& VulkanClipMatrix();
		// XZ 평면에 투영하는 지면 그림자 행렬을 생성한다.
		static glm::mat4 BuildGroundShadowMatrix(const glm::vec3& lightDir);
	};
}
