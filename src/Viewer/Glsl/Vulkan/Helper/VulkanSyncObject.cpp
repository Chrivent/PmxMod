#include "VulkanSyncObject.h"

#include <iostream>

namespace Chrivent {
	VulkanSyncObject::~VulkanSyncObject() {
		Destroy();
	}

	bool VulkanSyncObject::Initialize(const VulkanDeviceInfo& deviceInfo, const size_t swapChainImageCount) {
		device = deviceInfo.device;
		info.currentFrame = 0;
		ResetImageTracking(swapChainImageCount);
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (size_t i = 0; i < VulkanSyncObjectInfo::kMaxFramesInFlight; i++) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &info.imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &info.renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &info.inFlightFences[i]) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan sync objects.\n";
				Destroy();
				return false;
			}
		}
		return true;
	}

	void VulkanSyncObject::ResetImageTracking(const size_t swapChainImageCount) {
		info.imagesInFlight.assign(swapChainImageCount, VK_NULL_HANDLE);
	}

	void VulkanSyncObject::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		for (size_t i = 0; i < VulkanSyncObjectInfo::kMaxFramesInFlight; i++) {
			if (info.imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, info.imageAvailableSemaphores[i], nullptr);
				info.imageAvailableSemaphores[i] = VK_NULL_HANDLE;
			}
			if (info.renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, info.renderFinishedSemaphores[i], nullptr);
				info.renderFinishedSemaphores[i] = VK_NULL_HANDLE;
			}
			if (info.inFlightFences[i] != VK_NULL_HANDLE) {
				vkDestroyFence(device, info.inFlightFences[i], nullptr);
				info.inFlightFences[i] = VK_NULL_HANDLE;
			}
		}
		info.imagesInFlight.clear();
		info.currentFrame = 0;
		device = VK_NULL_HANDLE;
	}

	void VulkanSyncObject::AdvanceFrame() {
		info.currentFrame = (info.currentFrame + 1) % VulkanSyncObjectInfo::kMaxFramesInFlight;
	}
}
