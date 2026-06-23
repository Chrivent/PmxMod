#pragma once

#include "../Drawer.h"

#include <glm/glm.hpp>

namespace Chrivent {
	class VulkanInstance;

	class VulkanDrawer : public Drawer {
		VulkanInstance& instance;

	protected:
		// GL/DX와 같은 화면 좌표 및 깊이 범위로 맞추는 Vulkan clip 보정 행렬을 반환한다.
		const glm::mat4& ClipMatrix() const override;
		// 일반 메시 패스를 Vulkan으로 렌더링한다.
		void DrawModel() override;
		// 엣지 패스를 Vulkan으로 렌더링한다.
		void DrawEdge() override;
		// 지면 그림자 패스를 Vulkan으로 렌더링한다.
		void DrawGroundShadow() override;

	public:
		~VulkanDrawer() override = default;

		explicit VulkanDrawer(VulkanInstance& sourceInstance);
	};
}
