#pragma once

#include "../Drawer.h"

#include <glm/glm.hpp>

namespace Chrivent {
	struct VulkanInstanceInfo;

	class VulkanDrawer : public Drawer {
		VulkanInstanceInfo& info;

	protected:
		// 일반 메시 패스를 Vulkan으로 렌더링한다.
		void DrawModel() override;
		// 엣지 패스를 Vulkan으로 렌더링한다.
		void DrawEdge() override;
		// 지면 그림자 패스를 Vulkan으로 렌더링한다.
		void DrawGroundShadow() override;

	public:
		~VulkanDrawer() override = default;

		explicit VulkanDrawer(VulkanInstanceInfo& sourceInfo);
	};
}
