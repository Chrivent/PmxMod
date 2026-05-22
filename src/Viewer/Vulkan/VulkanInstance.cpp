#include "VulkanInstance.h"

#include "VulkanDrawer.h"
#include "VulkanViewer.h"

namespace Chrivent {
	VulkanInstance::VulkanInstance() {
		info = std::make_unique<VulkanInstanceInfo>();
		drawer = std::make_unique<VulkanDrawer>(static_cast<VulkanInstanceInfo&>(GetInfo()));
	}

	void VulkanInstance::Clear() {
	}

	bool VulkanInstance::Setup(Viewer& baseViewer) {
		static_cast<VulkanInstanceInfo&>(GetInfo()).viewer = dynamic_cast<VulkanViewer*>(&baseViewer);
		return false;
	}

	void VulkanInstance::Update() const {
	}
}
