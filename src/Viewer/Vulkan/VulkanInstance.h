#pragma once

#include "../Instance.h"

#include <vector>

namespace Chrivent {
	class VulkanViewer;
	struct VulkanMaterial;

	struct VulkanInstanceInfo : InstanceInfo {
		VulkanViewer* viewer = nullptr;
		std::vector<VulkanMaterial> materials;
	};

	class VulkanInstance : public Instance {
	protected:
		VulkanInstanceInfo& GetVulkanInfo() { return static_cast<VulkanInstanceInfo&>(GetInfo()); }
		const VulkanInstanceInfo& GetVulkanInfo() const { return static_cast<const VulkanInstanceInfo&>(GetInfo()); }

	public:
		VulkanInstance();
		~VulkanInstance() override = default;

		// Vulkan 紐⑤뜽 由ъ냼?ㅻ? ?댁젣?쒕떎.
		void Clear() override;
		// 紐⑤뜽 ?곗씠?곕? Vulkan 由ъ냼?ㅻ줈 ?낅줈?쒗븳??
		bool Setup(Viewer& baseViewer) override;
		// 紐⑤뜽??媛깆떊??踰꾪뀓???곗씠?곕? Vulkan 由ъ냼?ㅼ뿉 諛섏쁺?쒕떎.
		void Update() const override;
	};
}
