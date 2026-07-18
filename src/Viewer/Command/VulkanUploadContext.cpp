#include "Viewer/Command/VulkanUploadContext.h"

#include <iostream>
#include <limits>

namespace Chrivent {
	void VulkanUploadContext::Reset() {
		if (device != VK_NULL_HANDLE && fence != VK_NULL_HANDLE)
			vkDestroyFence(device, fence, nullptr);
		if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE)
			vkDestroyCommandPool(device, commandPool, nullptr);
		device = VK_NULL_HANDLE;
		commandPool = VK_NULL_HANDLE;
		commandBuffer = VK_NULL_HANDLE;
		fence = VK_NULL_HANDLE;
	}

	bool VulkanUploadContext::Initialize(const VulkanDevice& sourceDevice) {
		if (device == sourceDevice.device && commandPool != VK_NULL_HANDLE
			&& commandBuffer != VK_NULL_HANDLE && fence != VK_NULL_HANDLE)
			return true;
		Reset();
		device = sourceDevice.device;
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
			| VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = sourceDevice.queueFamilies.graphicsFamily;
		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan upload command pool.\n";
			Reset();
			return false;
		}
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool = commandPool;
		allocateInfo.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan upload command buffer.\n";
			Reset();
			return false;
		}
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan upload fence.\n";
			Reset();
			return false;
		}
		return true;
	}

	VulkanUploadContext::~VulkanUploadContext() {
		Reset();
	}

	bool VulkanUploadContext::Begin(const VulkanDevice& sourceDevice,
		VkCommandBuffer& targetCommandBuffer) {
		if (!Initialize(sourceDevice)
			|| vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
			return false;
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			std::cerr << "Failed to begin Vulkan upload command buffer.\n";
			return false;
		}
		targetCommandBuffer = commandBuffer;
		return true;
	}

	bool VulkanUploadContext::SubmitAndWait(const VulkanDevice& sourceDevice) const {
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to record Vulkan upload command buffer.\n";
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
			std::cerr << "Failed to submit Vulkan upload command buffer.\n";
			return false;
		}
		return vkWaitForFences(device, 1, &fence, VK_TRUE,
			std::numeric_limits<uint64_t>::max()) == VK_SUCCESS;
	}

	bool VulkanUploadContext::UploadIndexBuffer(const VulkanDevice& sourceDevice,
		const VkBuffer destination, const VkBuffer source, const VkDeviceSize size) {
		VkCommandBuffer targetCommandBuffer = VK_NULL_HANDLE;
		if (destination == VK_NULL_HANDLE || source == VK_NULL_HANDLE || size == 0
			|| !Begin(sourceDevice, targetCommandBuffer))
			return false;
		const VkBufferCopy copyRegion{ .size = size };
		vkCmdCopyBuffer(targetCommandBuffer, source, destination, 1, &copyRegion);
		const VkBufferMemoryBarrier2 barrier{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
			.dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = destination,
			.offset = 0,
			.size = size
		};
		const VkDependencyInfo dependencyInfo{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &barrier
		};
		vkCmdPipelineBarrier2(targetCommandBuffer, &dependencyInfo);
		return SubmitAndWait(sourceDevice);
	}
}
