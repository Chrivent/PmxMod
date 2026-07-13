#include "Viewer/Vulkan/VulkanPostProcess.h"

#include "Viewer/Vulkan/Helper/VulkanCommandBuffer.h"

#include <iostream>

namespace Chrivent {
	bool VulkanPostProcess::EndRecord(VulkanCommandBuffer& commandBuffers, const uint32_t imageIndex,
		const VkImage swapChainImage, const VkImageView swapChainImageView,
		const VkExtent2D extent, const bool sceneRenderingEnded) {
		const size_t targetImageCount = swapChainImageCount * targetCount;
		const size_t focusImageCount = swapChainImageCount * focusHistoryCount;
		if (imageIndex >= swapChainImageCount || imageIndex >= focusHistoryIndices.size()
			|| imageIndex >= focusHistoryInitialized.size() || targetImages.size() < targetImageCount
			|| targetImageViews.size() < targetImageCount || focusHistoryImages.size() < focusImageCount
			|| focusHistoryImageViews.size() < focusImageCount
			|| descriptorSets.size() < targetImageCount * focusHistoryCount
			|| focusHistoryDescriptorSets.size() < focusImageCount
			|| pipelines.empty() || pipelineLayout == VK_NULL_HANDLE)
			return false;
		const VkCommandBuffer commandBuffer = commandBuffers.ResolveCommandBuffer(imageIndex);
		if (commandBuffer == VK_NULL_HANDLE)
			return false;
		const VkImage sceneImage = targetImages[ResolveTargetIndex(0, imageIndex)];
		if (!sceneRenderingEnded) {
			vkCmdEndRendering(commandBuffer);
			VulkanCommandBuffer::TransitionImage(commandBuffer, sceneImage,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}

		const size_t focusHistoryReadIndex = focusHistoryIndices[imageIndex];
		const size_t focusHistoryWriteIndex = ResolveNextFocusHistoryIndex(focusHistoryReadIndex);
		const bool focusHistoryEnabled = focusHistoryPipeline != VK_NULL_HANDLE;
		if (focusHistoryEnabled) {
			constexpr VkImageSubresourceRange colorRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			};
			if (!focusHistoryInitialized[imageIndex]) {
				constexpr VkClearColorValue clearColor{};
				for (size_t historyIndex = 0; historyIndex < focusHistoryCount; historyIndex++) {
					const VkImage image = focusHistoryImages[ResolveFocusHistoryIndex(historyIndex, imageIndex)];
					VulkanCommandBuffer::TransitionImage(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
						VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
						VK_IMAGE_ASPECT_COLOR_BIT);
					vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						&clearColor, 1, &colorRange);
					VulkanCommandBuffer::TransitionImage(commandBuffer, image,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
						VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
						VK_IMAGE_ASPECT_COLOR_BIT);
				}
			}
			const size_t focusOutputIndex = ResolveFocusHistoryIndex(focusHistoryWriteIndex, imageIndex);
			const VkImage focusOutputImage = focusHistoryImages[focusOutputIndex];
			VulkanCommandBuffer::TransitionImage(commandBuffer, focusOutputImage,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
			const VkRenderingAttachmentInfo focusAttachment{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = focusHistoryImageViews[focusOutputIndex],
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE
			};
			constexpr VkExtent2D focusExtent{ 1, 1 };
			const VkRenderingInfo focusRenderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = { .extent = focusExtent },
				.layerCount = 1,
				.colorAttachmentCount = 1,
				.pColorAttachments = &focusAttachment
			};
			const VkDescriptorSet focusDescriptorSet =
				focusHistoryDescriptorSets[ResolveFocusHistoryIndex(focusHistoryReadIndex, imageIndex)];
			vkCmdBeginRendering(commandBuffer, &focusRenderingInfo);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, focusHistoryPipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 2, 1, &focusDescriptorSet, 0, nullptr);
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
			vkCmdEndRendering(commandBuffer);
			VulkanCommandBuffer::TransitionImage(commandBuffer, focusOutputImage,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}

		const size_t postProcessFocusIndex = focusHistoryEnabled
			? focusHistoryWriteIndex
			: focusHistoryReadIndex;
		for (size_t passIndex = 0; passIndex < pipelines.size(); passIndex++) {
			if (pipelines[passIndex] == VK_NULL_HANDLE)
				return false;
			const PostProcessPassRoute route = ResolvePingPongRoute(passIndex, pipelines.size());
			const VkImage outputImage = route.lastPass
				? swapChainImage
				: targetImages[ResolveTargetIndex(route.targetIndex, imageIndex)];
			const VkImageView outputImageView = route.lastPass
				? swapChainImageView
				: targetImageViews[ResolveTargetIndex(route.targetIndex, imageIndex)];
			VulkanCommandBuffer::TransitionImage(commandBuffer, outputImage, VK_IMAGE_LAYOUT_UNDEFINED,
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
			const VkDescriptorSet descriptorSet = descriptorSets[
				ResolveDescriptorIndex(route.sourceIndex, imageIndex, postProcessFocusIndex)];
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[passIndex]);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 2, 1, &descriptorSet, 0, nullptr);
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
			vkCmdEndRendering(commandBuffer);
			if (!route.lastPass) {
				VulkanCommandBuffer::TransitionImage(commandBuffer, outputImage,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_IMAGE_ASPECT_COLOR_BIT);
			}
		}
		VulkanCommandBuffer::TransitionImage(commandBuffer, swapChainImage,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_ASPECT_COLOR_BIT);
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to record Vulkan post-process command buffer.\n";
			return false;
		}
		if (focusHistoryEnabled)
			AdvanceFocusHistory(imageIndex);
		return true;
	}
}
