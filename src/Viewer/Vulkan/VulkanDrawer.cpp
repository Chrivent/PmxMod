#include "VulkanDrawer.h"

#include "VulkanInstance.h"
#include "VulkanViewer.h"

namespace Chrivent {
	VulkanDrawer::VulkanDrawer(const VulkanInstanceInfo& sourceInfo) : info(sourceInfo) {}

	void VulkanDrawer::DrawModel() const {
		if (info.viewer == nullptr)
			return;
		info.viewer->DrawIndexed(
			info.vertexBuffer.GetInfo(),
			info.indexBuffer.GetInfo(),
			info.indexType,
			info.indexCount);
	}

	void VulkanDrawer::DrawEdge() const {
	}

	void VulkanDrawer::DrawGroundShadow() const {
	}
}
