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

	bool VulkanSyncObject::RestoreCurrentFence() {
		VkFence& currentFence = inFlightFences[currentFrame];
		const VkFence discardedFence = currentFence;
		for (VkFence& imageFence : imagesInFlight) {
			if (imageFence == discardedFence)
				imageFence = VK_NULL_HANDLE;
		}
		if (discardedFence != VK_NULL_HANDLE)
			vkDestroyFence(device, discardedFence, nullptr);
		currentFence = VK_NULL_HANDLE;
		constexpr VkFenceCreateInfo fenceInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};
		return vkCreateFence(device, &fenceInfo, nullptr, &currentFence) == VK_SUCCESS;
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

	bool VulkanSyncObject::WaitForCurrentFrame() const {
		if (device == VK_NULL_HANDLE)
			return false;
		return vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX) == VK_SUCCESS;
	}

	bool VulkanSyncObject::WaitForImage(const uint32_t imageIndex) const {
		if (device == VK_NULL_HANDLE || imageIndex >= imagesInFlight.size())
			return false;
		const VkFence imageFence = imagesInFlight[imageIndex];
		return imageFence == VK_NULL_HANDLE
			|| vkWaitForFences(device, 1, &imageFence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
	}

	bool VulkanSyncObject::Submit(const VkQueue graphicsQueue, const VkCommandBuffer commandBuffer,
		const uint32_t imageIndex) {
		if (device == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE
			|| imageIndex >= renderFinishedSemaphores.size() || imageIndex >= imagesInFlight.size())
			return false;
		const VkSemaphoreSubmitInfo waitSemaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = imageAvailableSemaphores[currentFrame],
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		const VkSemaphoreSubmitInfo signalSemaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = renderFinishedSemaphores[imageIndex],
			.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
		};
		const VkCommandBufferSubmitInfo commandBufferInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = commandBuffer
		};
		const VkFence& inFlightFence = inFlightFences[currentFrame];
		if (vkResetFences(device, 1, &inFlightFence) != VK_SUCCESS)
			return false;
		const VkSubmitInfo2 submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = &waitSemaphoreInfo,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &commandBufferInfo,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos = &signalSemaphoreInfo
		};
		if (vkQueueSubmit2(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
			if (!RestoreCurrentFence())
				std::cerr << "Failed to restore Vulkan in-flight fence.\n";
			return false;
		}
		imagesInFlight[imageIndex] = inFlightFence;
		return true;
	}

	VkSemaphore VulkanSyncObject::GetRenderFinishedSemaphore(const uint32_t imageIndex) const {
		return imageIndex < renderFinishedSemaphores.size()
			? renderFinishedSemaphores[imageIndex] : VK_NULL_HANDLE;
	}
}
