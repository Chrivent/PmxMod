#include "VulkanCommandBuffer.h"

#include <array>
#include <iostream>

namespace Chrivent {
	VulkanCommandBuffer::~VulkanCommandBuffer() {
		Destroy();
	}

	bool VulkanCommandBuffer::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VkCommandPool sourceCommandPool,
		const VulkanSwapChainInfo& swapChainInfo) {
		device = deviceInfo.device;
		commandPool = sourceCommandPool;
		commandBuffers.resize(swapChainInfo.imageViews.size());
		if (commandBuffers.empty()) {
			std::cerr << "Failed to allocate Vulkan command buffers: swapchain image view is empty.\n";
			return false;
		}
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = commandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = commandBuffers.size();
		if (vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data()) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan command buffers.\n";
			return false;
		}
		return true;
	}

	bool VulkanCommandBuffer::Record(
		const uint32_t imageIndex,
		const VkRenderPass renderPass,
		const VkFramebuffer frameBuffer,
		const VkExtent2D extent,
		const float clearColor[4]) const {
		if (imageIndex >= commandBuffers.size()) {
			std::cerr << "Failed to record Vulkan command buffer: image index is out of range.\n";
			return false;
		}
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			std::cerr << "Failed to begin Vulkan command buffer.\n";
			return false;
		}
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {
			clearColor[0],
			clearColor[1],
			clearColor[2],
			clearColor[3]
		} };
		clearValues[1].depthStencil = { 1.0f, 0 };
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = frameBuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extent;
		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdEndRenderPass(commandBuffer);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to record Vulkan command buffer.\n";
			return false;
		}
		return true;
	}

	void VulkanCommandBuffer::Destroy() {
		if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE && !commandBuffers.empty())
			vkFreeCommandBuffers(device, commandPool, commandBuffers.size(), commandBuffers.data());
		commandBuffers.clear();
		device = VK_NULL_HANDLE;
		commandPool = VK_NULL_HANDLE;
	}
}
