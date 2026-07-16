#include "Viewer/Synchronization/VulkanSyncObject.h"

#include <iostream>

namespace Chrivent {
	bool VulkanSyncObject::CreateRenderFinishedSemaphores(const size_t swapChainImageCount) {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		renderFinishedSemaphores.assign(swapChainImageCount, VK_NULL_HANDLE);
		for (auto& semaphore : renderFinishedSemaphores) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan present semaphore.\n";
				ResetRenderFinishedSemaphores();
				return false;
			}
		}
		return true;
	}

	void VulkanSyncObject::ResetRenderFinishedSemaphores() {
		if (device != VK_NULL_HANDLE) {
			for (const VkSemaphore semaphore : renderFinishedSemaphores) {
				if (semaphore != VK_NULL_HANDLE)
					vkDestroySemaphore(device, semaphore, nullptr);
			}
		}
		renderFinishedSemaphores.clear();
	}

	VulkanSyncObject::~VulkanSyncObject() {
		Reset();
	}

	bool VulkanSyncObject::Initialize(const VulkanDevice& sourceDevice, const size_t swapChainImageCount) {
		device = sourceDevice.device;
		currentFrame = 0;
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (size_t i = 0; i < FrameBuffering::vulkanFramesInFlight; i++) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan sync objects.\n";
				Reset();
				return false;
			}
		}
		return ResetImageTracking(swapChainImageCount);
	}

	void VulkanSyncObject::Reset() {
		if (device == VK_NULL_HANDLE)
			return;
		ResetRenderFinishedSemaphores();
		for (size_t i = 0; i < FrameBuffering::vulkanFramesInFlight; i++) {
			if (imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
				imageAvailableSemaphores[i] = VK_NULL_HANDLE;
			}
			if (inFlightFences[i] != VK_NULL_HANDLE) {
				vkDestroyFence(device, inFlightFences[i], nullptr);
				inFlightFences[i] = VK_NULL_HANDLE;
			}
		}
		imagesInFlight.clear();
		currentFrame = 0;
		device = VK_NULL_HANDLE;
	}

	bool VulkanSyncObject::ResetImageTracking(const size_t swapChainImageCount) {
		ResetRenderFinishedSemaphores();
		imagesInFlight.assign(swapChainImageCount, VK_NULL_HANDLE);
		return CreateRenderFinishedSemaphores(swapChainImageCount);
	}
}
