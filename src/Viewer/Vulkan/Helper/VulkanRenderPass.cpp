#include "VulkanRenderPass.h"

#include <iostream>

namespace Chrivent {
	VulkanRenderPass::~VulkanRenderPass() {
		Destroy();
	}

	bool VulkanRenderPass::Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo) {
		device = deviceInfo.device;
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainInfo.imageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		VkAttachmentReference colorAttachmentRef;
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		VkRenderPassCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = 1;
		createInfo.pAttachments = &colorAttachment;
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpass;
		createInfo.dependencyCount = 1;
		createInfo.pDependencies = &dependency;
		if (vkCreateRenderPass(deviceInfo.device, &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan render pass.\n";
			return false;
		}
		return true;
	}

	void VulkanRenderPass::Destroy() {
		if (device != VK_NULL_HANDLE && renderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(device, renderPass, nullptr);
			renderPass = VK_NULL_HANDLE;
		}
		device = VK_NULL_HANDLE;
	}
}
