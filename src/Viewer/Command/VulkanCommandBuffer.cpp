#include "Viewer/Command/VulkanCommandBuffer.h"

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

	void VulkanCommandBuffer::ApplyViewportAndScissor(
		const VkCommandBuffer commandBuffer, const VkExtent2D extent) {
		const VkViewport viewport{
			.width = static_cast<float>(extent.width),
			.height = static_cast<float>(extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};
		const VkRect2D scissor{ .extent = extent };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	VulkanCommandBuffer::~VulkanCommandBuffer() {
		Reset();
	}

	bool VulkanCommandBuffer::Initialize(const VulkanDevice& sourceDevice, const VkCommandPool sourceCommandPool, const VulkanSwapChain& sourceSwapChain) {
		device = sourceDevice.GetDevice();
		commandPool = sourceCommandPool;
		commandBuffers.resize(sourceSwapChain.GetImageCount());
		if (commandBuffers.empty()) {
			std::cerr << "swap chain image view가 없어 Vulkan command buffer를 할당할 수 없습니다.\n";
			return false;
		}
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = commandPool;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = commandBuffers.size();
		if (vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data()) != VK_SUCCESS) {
			std::cerr << "Vulkan command buffer를 할당하지 못했습니다.\n";
			return false;
		}
		return true;
	}

	bool VulkanCommandBuffer::BeginRecord(const uint32_t imageIndex,
		const VkImage colorImage, const VkImageView colorImageView,
		const VkImage resolveImage, const VkImageView resolveImageView,
		const VkImage depthImage, const VkImageView depthImageView,
		const bool depthHasStencil, const VkSampleCountFlagBits sampleCount,
		const VkExtent2D extent, const float clearColor[4]) const {
		if (imageIndex >= commandBuffers.size()) {
			std::cerr << "이미지 색인이 범위를 벗어나 Vulkan command buffer를 기록할 수 없습니다.\n";
			return false;
		}
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			std::cerr << "Vulkan command buffer 기록을 시작하지 못했습니다.\n";
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
		ApplyViewportAndScissor(commandBuffer, extent);
		return true;
	}

	void VulkanCommandBuffer::BindPipeline(const uint32_t imageIndex, const VkPipeline pipeline) const {
		if (imageIndex >= commandBuffers.size() || pipeline == VK_NULL_HANDLE)
			return;
		vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	}

	bool VulkanCommandBuffer::BindVertexBuffer(const uint32_t imageIndex, const VkBuffer vertexBuffer) const {
		if (imageIndex >= commandBuffers.size() || vertexBuffer == VK_NULL_HANDLE)
			return false;
		constexpr VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 1, &vertexBuffer, &offset);
		return true;
	}

	bool VulkanCommandBuffer::BindIndexBuffer(const uint32_t imageIndex,
		const VkBuffer indexBuffer, const VkIndexType indexType) const {
		if (imageIndex >= commandBuffers.size() || indexBuffer == VK_NULL_HANDLE)
			return false;
		vkCmdBindIndexBuffer(commandBuffers[imageIndex], indexBuffer, 0, indexType);
		return true;
	}

	bool VulkanCommandBuffer::DrawIndexed(const uint32_t imageIndex,
		const uint32_t firstIndex, const uint32_t indexCount) const {
		if (imageIndex >= commandBuffers.size())
			return false;
		if (indexCount != 0)
			vkCmdDrawIndexed(commandBuffers[imageIndex], indexCount, 1, firstIndex, 0, 0);
		return true;
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

	bool VulkanCommandBuffer::EndRendering(const uint32_t imageIndex) const {
		if (imageIndex >= commandBuffers.size())
			return false;
		vkCmdEndRendering(commandBuffers[imageIndex]);
		return true;
	}

	bool VulkanCommandBuffer::EndSceneColorPass(const uint32_t imageIndex, const VkImage sceneImage) const {
		if (sceneImage == VK_NULL_HANDLE || !EndRendering(imageIndex))
			return false;
		TransitionImage(commandBuffers[imageIndex], sceneImage,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT);
		return true;
	}

	bool VulkanCommandBuffer::BeginPostProcessSceneInputPass(
		const uint32_t imageIndex, const VkImage sceneImage, const VkImage depthImage,
		const VkImageView depthImageView, const VkImage velocityImage, const VkImageView velocityImageView,
		const bool velocityInitialized, const bool depthHasStencil, const VkExtent2D extent) const {
		if (imageIndex >= commandBuffers.size() || depthImage == VK_NULL_HANDLE ||
			depthImageView == VK_NULL_HANDLE || !EndSceneColorPass(imageIndex, sceneImage))
			return false;
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		const VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT | (depthHasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
		TransitionImage(commandBuffer, depthImage, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, depthAspect);
		if (velocityImage != VK_NULL_HANDLE) {
			TransitionImage(commandBuffer, velocityImage,
				velocityInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				velocityInitialized ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_2_NONE,
				velocityInitialized ? VK_ACCESS_2_SHADER_SAMPLED_READ_BIT : VK_ACCESS_2_NONE,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}
		constexpr VkClearValue depthClear{ .depthStencil = { 1.0f, 0 } };
		const VkRenderingAttachmentInfo depthAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = depthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = depthClear
		};
		constexpr VkClearValue velocityClear{};
		const VkRenderingAttachmentInfo velocityAttachment{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = velocityImageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = velocityClear
		};
		const VkRenderingInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = { .extent = extent },
			.layerCount = 1,
			.colorAttachmentCount = velocityImageView != VK_NULL_HANDLE ? 1u : 0u,
			.pColorAttachments = velocityImageView != VK_NULL_HANDLE ? &velocityAttachment : nullptr,
			.pDepthAttachment = &depthAttachment,
			.pStencilAttachment = depthHasStencil ? &depthAttachment : nullptr
		};
		vkCmdBeginRendering(commandBuffer, &renderingInfo);
		ApplyViewportAndScissor(commandBuffer, extent);
		return true;
	}

	bool VulkanCommandBuffer::EndPostProcessSceneInputPass(
		const uint32_t imageIndex, const VkImage depthImage,
		const VkImage velocityImage, const bool depthHasStencil) const {
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
		if (velocityImage != VK_NULL_HANDLE) {
			TransitionImage(commandBuffer, velocityImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}
		return true;
	}

	bool VulkanCommandBuffer::EndRecord(const uint32_t imageIndex, const VkImage outputImage) const {
		if (imageIndex >= commandBuffers.size()) {
			std::cerr << "이미지 색인이 범위를 벗어나 Vulkan command buffer를 끝낼 수 없습니다.\n";
			return false;
		}
		const VkCommandBuffer commandBuffer = commandBuffers[imageIndex];
		TransitionImage(commandBuffer, outputImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
			VK_IMAGE_ASPECT_COLOR_BIT);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			std::cerr << "Vulkan command buffer를 기록하지 못했습니다.\n";
			return false;
		}
		return true;
	}

	void VulkanCommandBuffer::Reset() {
		if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE && !commandBuffers.empty())
			vkFreeCommandBuffers(device, commandPool, commandBuffers.size(), commandBuffers.data());
		commandBuffers.clear();
		device = VK_NULL_HANDLE;
		commandPool = VK_NULL_HANDLE;
	}
}
