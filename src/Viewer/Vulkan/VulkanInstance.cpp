#include "VulkanInstance.h"

#include "VulkanDrawer.h"
#include "VulkanViewer.h"

namespace Chrivent {
	VulkanInstance::VulkanInstance() {
		info = std::make_unique<VulkanInstanceInfo>();
		drawer = std::make_unique<VulkanDrawer>(GetVulkanInfo());
	}

	void VulkanInstance::Clear() {
	}

	bool VulkanInstance::Setup(Viewer& baseViewer) {
		GetVulkanInfo().viewer = dynamic_cast<VulkanViewer*>(&baseViewer);
		return false;
	}

	void VulkanInstance::Update() const {
	}
}
