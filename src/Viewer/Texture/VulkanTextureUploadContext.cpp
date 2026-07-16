#include "Viewer/Texture/VulkanTextureUploadContext.h"

#include <iostream>
#include <limits>

namespace Chrivent {
	void VulkanTextureUploadContext::Reset() {
		if (device != VK_NULL_HANDLE && commandBuffer != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE)
			vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		if (device != VK_NULL_HANDLE && fence != VK_NULL_HANDLE)
			vkDestroyFence(device, fence, nullptr);
		device = VK_NULL_HANDLE;
		commandPool = VK_NULL_HANDLE;
		commandBuffer = VK_NULL_HANDLE;
		fence = VK_NULL_HANDLE;
	}

	bool VulkanTextureUploadContext::Initialize(const VulkanDevice& sourceDevice,
		const VkCommandPool sourceCommandPool) {
		if (device == sourceDevice.device && commandPool == sourceCommandPool
			&& commandBuffer != VK_NULL_HANDLE && fence != VK_NULL_HANDLE)
			return true;
		Reset();
		device = sourceDevice.device;
		commandPool = sourceCommandPool;
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool = commandPool;
		allocateInfo.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan texture command buffer.\n";
			Reset();
			return false;
		}
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan texture upload fence.\n";
			Reset();
			return false;
		}
		return true;
	}

	VulkanTextureUploadContext::~VulkanTextureUploadContext() {
		Reset();
	}

	bool VulkanTextureUploadContext::Begin(const VulkanDevice& sourceDevice,
		const VkCommandPool sourceCommandPool, VkCommandBuffer& targetCommandBuffer) {
		if (!Initialize(sourceDevice, sourceCommandPool)
			|| vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
			return false;
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			std::cerr << "Failed to begin Vulkan texture command buffer.\n";
			return false;
		}
		targetCommandBuffer = commandBuffer;
		return true;
	}

	bool VulkanTextureUploadContext::SubmitAndWait(const VulkanDevice& sourceDevice) const {
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to record Vulkan texture command buffer.\n";
			return false;
		}
		if (vkResetFences(device, 1, &fence) != VK_SUCCESS)
			return false;
		const VkCommandBufferSubmitInfo commandBufferInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = commandBuffer
		};
		const VkSubmitInfo2 submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &commandBufferInfo
		};
		if (vkQueueSubmit2(sourceDevice.graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
			std::cerr << "Failed to submit Vulkan texture command buffer.\n";
			return false;
		}
		return vkWaitForFences(device, 1, &fence, VK_TRUE,
			std::numeric_limits<uint64_t>::max()) == VK_SUCCESS;
	}
}
