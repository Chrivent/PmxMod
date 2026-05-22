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
	public:
		VulkanInstance();
		~VulkanInstance() override = default;

		// Vulkan 모델 리소스를 해제한다.
		void Clear() override;
		// 모델 데이터를 Vulkan 리소스로 업로드한다.
		bool Setup(Viewer& baseViewer) override;
		// 모델의 갱신된 버텍스 데이터를 Vulkan 리소스에 반영한다.
		void Update() const override;
	};
}
