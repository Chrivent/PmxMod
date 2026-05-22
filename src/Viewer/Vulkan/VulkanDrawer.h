#pragma once

#include "../Drawer.h"

namespace Chrivent {
	struct VulkanInstanceInfo;

	class VulkanDrawer : public Drawer {
		const VulkanInstanceInfo& info;

	protected:
		// ?쇰컲 硫붿떆 ?⑥뒪瑜?Vulkan?쇰줈 ?뚮뜑留곹븳??
		void DrawModel() const override;
		// ?ｌ? ?⑥뒪瑜?Vulkan?쇰줈 ?뚮뜑留곹븳??
		void DrawEdge() const override;
		// 吏硫?洹몃┝???⑥뒪瑜?Vulkan?쇰줈 ?뚮뜑留곹븳??
		void DrawGroundShadow() const override;

	public:
		explicit VulkanDrawer(const VulkanInstanceInfo& sourceInfo);
		~VulkanDrawer() override = default;
	};
}
