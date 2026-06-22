#include "VulkanSyncObject.h"

#include <iostream>

namespace Chrivent {
	bool VulkanSyncObject::CreateRenderFinishedSemaphores(const size_t swapChainImageCount) {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		info.renderFinishedSemaphores.assign(swapChainImageCount, VK_NULL_HANDLE);
		for (auto& semaphore : info.renderFinishedSemaphores) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan present semaphore.\n";
				DestroyRenderFinishedSemaphores();
				return false;
			}
		}
		return true;
	}

	void VulkanSyncObject::DestroyRenderFinishedSemaphores() {
		if (device != VK_NULL_HANDLE) {
			for (const VkSemaphore semaphore : info.renderFinishedSemaphores) {
				if (semaphore != VK_NULL_HANDLE)
					vkDestroySemaphore(device, semaphore, nullptr);
			}
		}
		info.renderFinishedSemaphores.clear();
	}

	VulkanSyncObject::~VulkanSyncObject() {
		Destroy();
	}

	bool VulkanSyncObject::Initialize(const VulkanDeviceInfo& deviceInfo, const size_t swapChainImageCount) {
		device = deviceInfo.device;
		info.currentFrame = 0;
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (size_t i = 0; i < VulkanSyncObjectInfo::kMaxFramesInFlight; i++) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &info.imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &info.inFlightFences[i]) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan sync objects.\n";
				Destroy();
				return false;
			}
		}
		return ResetImageTracking(swapChainImageCount);
	}

	void VulkanSyncObject::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		DestroyRenderFinishedSemaphores();
		for (size_t i = 0; i < VulkanSyncObjectInfo::kMaxFramesInFlight; i++) {
			if (info.imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, info.imageAvailableSemaphores[i], nullptr);
				info.imageAvailableSemaphores[i] = VK_NULL_HANDLE;
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

	bool VulkanSyncObject::ResetImageTracking(const size_t swapChainImageCount) {
		DestroyRenderFinishedSemaphores();
		info.imagesInFlight.assign(swapChainImageCount, VK_NULL_HANDLE);
		return CreateRenderFinishedSemaphores(swapChainImageCount);
	}

	void VulkanSyncObject::AdvanceFrame() {
		info.currentFrame = (info.currentFrame + 1) % VulkanSyncObjectInfo::kMaxFramesInFlight;
	}
}
