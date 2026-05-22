#pragma once

#include "../Drawer.h"

namespace Chrivent {
	struct VulkanInstanceInfo;

	class VulkanDrawer : public Drawer {
		const VulkanInstanceInfo& info;

	protected:
		// 일반 메시 패스를 Vulkan으로 렌더링한다.
		void DrawModel() const override;
		// 엣지 패스를 Vulkan으로 렌더링한다.
		void DrawEdge() const override;
		// 지면 그림자 패스를 Vulkan으로 렌더링한다.
		void DrawGroundShadow() const override;

	public:
		explicit VulkanDrawer(const VulkanInstanceInfo& sourceInfo);
		~VulkanDrawer() override = default;
	};
}
