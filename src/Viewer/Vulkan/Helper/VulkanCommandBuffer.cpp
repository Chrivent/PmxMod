#include "VulkanCommandBuffer.h"

#include <array>
#include <iostream>

namespace Chrivent {
	VulkanCommandBuffer::~VulkanCommandBuffer() {
		Destroy();
	}

	VkCommandBuffer VulkanCommandBuffer::ResolveCommandBuffer(const uint32_t imageIndex) const {
		return commandBuffers[imageIndex];
	}

	bool VulkanCommandBuffer::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VkCommandPool sourceCommandPool,
		const VulkanSwapChainInfo& swapChainInfo) {
		device = deviceInfo.device;
		commandPool = sourceCommandPool;
		commandBuffers.resize(swapChainInfo.imageViews.size());
		boundVertexBuffers.assign(commandBuffers.size(), VK_NULL_HANDLE);
		boundIndexBuffers.assign(commandBuffers.size(), VK_NULL_HANDLE);
		boundIndexTypes.assign(commandBuffers.size(), VK_INDEX_TYPE_MAX_ENUM);
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

	bool VulkanCommandBuffer::BeginRecord(
		const uint32_t imageIndex,
		const VkRenderPass renderPass,
		const VkFramebuffer frameBuffer,
		const VkPipeline pipeline,
		const VkExtent2D extent,
		const float clearColor[4]) {
		if (imageIndex >= commandBuffers.size()) {
			std::cerr << "Failed to record Vulkan command buffer: image index is out of range.\n";
			return false;
		}
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		boundVertexBuffers[imageIndex] = VK_NULL_HANDLE;
		boundIndexBuffers[imageIndex] = VK_NULL_HANDLE;
		boundIndexTypes[imageIndex] = VK_INDEX_TYPE_MAX_ENUM;
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
		if (pipeline != VK_NULL_HANDLE)
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		return true;
	}

	void VulkanCommandBuffer::BindPipeline(const uint32_t imageIndex, const VkPipeline pipeline) const {
		if (imageIndex >= commandBuffers.size() || pipeline == VK_NULL_HANDLE)
			return;
		vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	}

	void VulkanCommandBuffer::DrawIndexed(
		const uint32_t imageIndex,
		const VulkanBufferInfo& vertexBuffer,
		const VulkanBufferInfo& indexBuffer,
		const VkIndexType indexType,
		const uint32_t firstIndex,
		const uint32_t indexCount) {
		if (imageIndex >= commandBuffers.size() ||
			vertexBuffer.buffer == VK_NULL_HANDLE ||
			indexBuffer.buffer == VK_NULL_HANDLE ||
			indexCount == 0)
			return;
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		if (boundVertexBuffers[imageIndex] != vertexBuffer.buffer) {
			constexpr VkDeviceSize offsets[] = { 0 };
			const VkBuffer vertexBuffers[] = { vertexBuffer.buffer };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			boundVertexBuffers[imageIndex] = vertexBuffer.buffer;
		}
		if (boundIndexBuffers[imageIndex] != indexBuffer.buffer ||
			boundIndexTypes[imageIndex] != indexType) {
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, indexType);
			boundIndexBuffers[imageIndex] = indexBuffer.buffer;
			boundIndexTypes[imageIndex] = indexType;
		}
		vkCmdDrawIndexed(commandBuffer, indexCount, 1, firstIndex, 0, 0);
	}

	void VulkanCommandBuffer::BindDescriptorSets(
		const uint32_t imageIndex,
		const VkPipelineLayout pipelineLayout,
		const uint32_t firstSet,
		const VkDescriptorSet* descriptorSets,
		const uint32_t descriptorSetCount,
		const uint32_t* dynamicOffsets,
		const uint32_t dynamicOffsetCount) const {
		if (imageIndex >= commandBuffers.size() ||
			pipelineLayout == VK_NULL_HANDLE ||
			descriptorSets == nullptr ||
			descriptorSetCount == 0)
			return;
		vkCmdBindDescriptorSets(
			commandBuffers[imageIndex],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			firstSet,
			descriptorSetCount,
			descriptorSets,
			dynamicOffsetCount,
			dynamicOffsets);
	}

	bool VulkanCommandBuffer::EndRecord(const uint32_t imageIndex) const {
		if (imageIndex >= commandBuffers.size()) {
			std::cerr << "Failed to end Vulkan command buffer: image index is out of range.\n";
			return false;
		}
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
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
		boundVertexBuffers.clear();
		boundIndexBuffers.clear();
		boundIndexTypes.clear();
		device = VK_NULL_HANDLE;
		commandPool = VK_NULL_HANDLE;
	}
}
