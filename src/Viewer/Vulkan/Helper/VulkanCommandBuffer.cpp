#include "Viewer/Vulkan/Helper/VulkanCommandBuffer.h"

#include <iostream>

namespace Chrivent {
	void VulkanCommandBuffer::TransitionImage(const VkCommandBuffer commandBuffer, const VkImage image,
		const VkImageLayout oldLayout, const VkImageLayout newLayout,
		const VkPipelineStageFlags2 sourceStage, const VkAccessFlags2 sourceAccess,
		const VkPipelineStageFlags2 destinationStage, const VkAccessFlags2 destinationAccess,
		const VkImageAspectFlags aspectMask) {
		const VkImageMemoryBarrier2 barrier{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = sourceStage,
			.srcAccessMask = sourceAccess,
			.dstStageMask = destinationStage,
			.dstAccessMask = destinationAccess,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
				.aspectMask = aspectMask,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};
		const VkDependencyInfo dependencyInfo{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};
		vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
	}

	VulkanCommandBuffer::~VulkanCommandBuffer() {
		Reset();
	}

	bool VulkanCommandBuffer::Initialize(const VulkanDevice& sourceDevice, const VkCommandPool sourceCommandPool, const VulkanSwapChain& sourceSwapChain) {
		device = sourceDevice.device;
		commandPool = sourceCommandPool;
		commandBuffers.resize(sourceSwapChain.imageViews.size());
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

	bool VulkanCommandBuffer::BeginRecord(const uint32_t imageIndex,
		const VkImage colorImage, const VkImageView colorImageView,
		const VkImage resolveImage, const VkImageView resolveImageView,
		const VkImage depthImage, const VkImageView depthImageView,
		const bool depthHasStencil, const VkSampleCountFlagBits sampleCount, const VkPipeline pipeline,
		const VkExtent2D extent, const float clearColor[4]) {
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
		const bool multisampled = sampleCount != VK_SAMPLE_COUNT_1_BIT;
		const VkImage renderColorImage = multisampled ? colorImage : resolveImage;
		const VkImageView renderColorView = multisampled ? colorImageView : resolveImageView;
		TransitionImage(commandBuffer, renderColorImage, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT);
		if (multisampled) {
			TransitionImage(commandBuffer, resolveImage, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}
		TransitionImage(commandBuffer, depthImage, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT | (depthHasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0));
		const VkClearValue colorClear{ .color = { {
			clearColor[0], clearColor[1], clearColor[2], clearColor[3]
		} } };
		constexpr VkClearValue depthClear{ .depthStencil = { 1.0f, 0 } };
		const VkRenderingAttachmentInfo colorAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = renderColorView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = multisampled ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
			.resolveImageView = multisampled ? resolveImageView : VK_NULL_HANDLE,
			.resolveImageLayout = multisampled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = colorClear
		};
		const VkRenderingAttachmentInfo depthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = depthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue = depthClear
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = { .extent = extent },
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachment,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = depthHasStencil ? &depthAttachment : nullptr
		};
		vkCmdBeginRendering(commandBuffer, &renderingInfo);
		if (pipeline != VK_NULL_HANDLE)
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		return true;
	}

	void VulkanCommandBuffer::BindPipeline(const uint32_t imageIndex, const VkPipeline pipeline) const {
		if (imageIndex >= commandBuffers.size() || pipeline == VK_NULL_HANDLE)
			return;
		vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	}

	void VulkanCommandBuffer::DrawIndexed(const uint32_t imageIndex, const VulkanBuffer& vertexBuffer,
		const VulkanBuffer& indexBuffer, const VkIndexType indexType, const uint32_t firstIndex, const uint32_t indexCount) {
		if (imageIndex >= commandBuffers.size() || vertexBuffer.buffer == VK_NULL_HANDLE || indexBuffer.buffer == VK_NULL_HANDLE || indexCount == 0)
			return;
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		if (boundVertexBuffers[imageIndex] != vertexBuffer.buffer) {
			constexpr VkDeviceSize offsets[] = { 0 };
			const VkBuffer vertexBuffers[] = { vertexBuffer.buffer };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			boundVertexBuffers[imageIndex] = vertexBuffer.buffer;
		}
		if (boundIndexBuffers[imageIndex] != indexBuffer.buffer || boundIndexTypes[imageIndex] != indexType) {
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, indexType);
			boundIndexBuffers[imageIndex] = indexBuffer.buffer;
			boundIndexTypes[imageIndex] = indexType;
		}
		vkCmdDrawIndexed(commandBuffer, indexCount, 1, firstIndex, 0, 0);
	}

	void VulkanCommandBuffer::BindDescriptorSets(const uint32_t imageIndex, const VkPipelineLayout pipelineLayout,
		const uint32_t firstSet, const std::span<const VkDescriptorSet> descriptorSets, const std::span<const uint32_t> dynamicOffsets) const {
		if (imageIndex >= commandBuffers.size() ||
			pipelineLayout == VK_NULL_HANDLE ||
			descriptorSets.empty())
			return;
		vkCmdBindDescriptorSets(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
			firstSet, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
			static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
	}

	bool VulkanCommandBuffer::BeginPostProcessDepthPass(
		const uint32_t imageIndex, const VkImage sceneImage, const VkImage depthImage,
		const VkImageView depthImageView, const bool depthHasStencil, const VkPipeline pipeline,
		const VkExtent2D extent) const {
		if (imageIndex >= commandBuffers.size() || depthImage == VK_NULL_HANDLE ||
			depthImageView == VK_NULL_HANDLE || pipeline == VK_NULL_HANDLE)
			return false;
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		vkCmdEndRendering(commandBuffer);
		TransitionImage(commandBuffer, sceneImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		const VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT | (depthHasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		TransitionImage(commandBuffer, depthImage, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, depthAspect);
		constexpr VkClearValue depthClear{ .depthStencil = { 1.0f, 0 } };
		const VkRenderingAttachmentInfo depthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = depthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = depthClear
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = { .extent = extent },
			.layerCount = 1,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = depthHasStencil ? &depthAttachment : nullptr
		};
		vkCmdBeginRendering(commandBuffer, &renderingInfo);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		return true;
	}

	bool VulkanCommandBuffer::EndPostProcessDepthPass(
		const uint32_t imageIndex, const VkImage depthImage, const bool depthHasStencil) const {
		if (imageIndex >= commandBuffers.size() || depthImage == VK_NULL_HANDLE)
			return false;
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		vkCmdEndRendering(commandBuffer);
		const VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT | (depthHasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		TransitionImage(commandBuffer, depthImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, depthAspect);
		return true;
	}

	bool VulkanCommandBuffer::EndRecord(const uint32_t imageIndex, const VkImage outputImage) const {
		if (imageIndex >= commandBuffers.size()) {
			std::cerr << "Failed to end Vulkan command buffer: image index is out of range.\n";
			return false;
		}
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		vkCmdEndRendering(commandBuffer);
		TransitionImage(commandBuffer, outputImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
			VK_IMAGE_ASPECT_COLOR_BIT);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to record Vulkan command buffer.\n";
			return false;
		}
		return true;
	}

	bool VulkanCommandBuffer::EndRecordWithPostProcess(const uint32_t imageIndex,
		const VkImage sceneImage, const VkImage swapChainImage, const VkImageView swapChainImageView,
		const std::span<const VkImage> targetImages, const std::span<const VkImageView> targetImageViews,
		const std::span<const VkPipeline> pipelines,
		const VkPipelineLayout pipelineLayout, const std::span<const VkDescriptorSet> descriptorSets,
		const VkExtent2D extent, const bool sceneRenderingEnded) const {
		if (imageIndex >= commandBuffers.size() || pipelines.empty()
			|| pipelineLayout == VK_NULL_HANDLE || targetImages.size() < 3
			|| targetImageViews.size() < 3 || descriptorSets.size() < 3)
			return false;
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		if (!sceneRenderingEnded) {
			vkCmdEndRendering(commandBuffer);
			TransitionImage(commandBuffer, sceneImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
		}
		for (size_t passIndex = 0; passIndex < pipelines.size(); passIndex++) {
			if (pipelines[passIndex] == VK_NULL_HANDLE)
				return false;
			const bool lastPass = passIndex + 1 == pipelines.size();
			const size_t sourceIndex = passIndex == 0 ? 0 : (passIndex - 1) % 2 + 1;
			const size_t targetIndex = passIndex % 2 + 1;
			const VkImage outputImage = lastPass ? swapChainImage : targetImages[targetIndex];
			const VkImageView outputImageView = lastPass ? swapChainImageView : targetImageViews[targetIndex];
			TransitionImage(commandBuffer, outputImage, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
			const VkRenderingAttachmentInfo colorAttachment{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = outputImageView,
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE
			};
			const VkRenderingInfo renderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = { .extent = extent },
				.layerCount = 1,
				.colorAttachmentCount = 1,
				.pColorAttachments = &colorAttachment
			};
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[passIndex]);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 2, 1, &descriptorSets[sourceIndex], 0, nullptr);
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
			vkCmdEndRendering(commandBuffer);
			if (!lastPass) {
				TransitionImage(commandBuffer, outputImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
					VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}
		TransitionImage(commandBuffer, swapChainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
			VK_IMAGE_ASPECT_COLOR_BIT);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to record Vulkan post-process command buffer.\n";
			return false;
		}
		return true;
	}

	void VulkanCommandBuffer::Reset() {
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
