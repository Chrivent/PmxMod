#include "VulkanSyncObject.h"

#include <iostream>

namespace Chrivent {
	VulkanSyncObject::~VulkanSyncObject() {
		Destroy();
	}

	bool VulkanSyncObject::Initialize(const VulkanDeviceInfo& deviceInfo) {
		device = deviceInfo.device;
		currentFrame = 0;
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (size_t i = 0; i < kMaxFramesInFlight; i++) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan sync objects.\n";
				Destroy();
				return false;
			}
		}
		return true;
	}

	void VulkanSyncObject::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		for (size_t i = 0; i < kMaxFramesInFlight; i++) {
			if (imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
				imageAvailableSemaphores[i] = VK_NULL_HANDLE;
			}
			if (renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
				renderFinishedSemaphores[i] = VK_NULL_HANDLE;
			}
			if (inFlightFences[i] != VK_NULL_HANDLE) {
				vkDestroyFence(device, inFlightFences[i], nullptr);
				inFlightFences[i] = VK_NULL_HANDLE;
			}
		}
		currentFrame = 0;
		device = VK_NULL_HANDLE;
	}

	void VulkanSyncObject::AdvanceFrame() {
		currentFrame = (currentFrame + 1) % kMaxFramesInFlight;
	}
}
