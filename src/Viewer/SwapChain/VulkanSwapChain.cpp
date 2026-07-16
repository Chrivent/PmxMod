#include "Viewer/SwapChain/VulkanSwapChain.h"

#include <algorithm>
#include <iostream>

namespace Chrivent {
	bool VulkanSwapChain::QuerySupport(const VulkanDevice& sourceDevice, Support& support) {
		support = {};
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			sourceDevice.physicalDevice, sourceDevice.surface, &support.capabilities) != VK_SUCCESS)
			return false;
		uint32_t formatCount = 0;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(
			sourceDevice.physicalDevice, sourceDevice.surface, &formatCount, nullptr) != VK_SUCCESS)
			return false;
		if (formatCount > 0) {
			support.formats.resize(formatCount);
			if (vkGetPhysicalDeviceSurfaceFormatsKHR(sourceDevice.physicalDevice, sourceDevice.surface,
				&formatCount, support.formats.data()) != VK_SUCCESS)
				return false;
			support.formats.resize(formatCount);
		}
		uint32_t presentModeCount = 0;
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(
			sourceDevice.physicalDevice, sourceDevice.surface, &presentModeCount, nullptr) != VK_SUCCESS)
			return false;
		if (presentModeCount > 0) {
			support.presentModes.resize(presentModeCount);
			if (vkGetPhysicalDeviceSurfacePresentModesKHR(sourceDevice.physicalDevice, sourceDevice.surface,
				&presentModeCount, support.presentModes.data()) != VK_SUCCESS)
				return false;
			support.presentModes.resize(presentModeCount);
		}
		return true;
	}

	VkSurfaceFormatKHR VulkanSwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}
		return formats.front();
	}

	VkPresentModeKHR VulkanSwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
		for (const VkPresentModeKHR mode : presentModes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				return mode;
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanSwapChain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D extent{
			static_cast<uint32_t>(std::max(0, width)),
			static_cast<uint32_t>(std::max(0, height))
		};
		extent.width = (std::clamp)(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = (std::clamp)(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return extent;
	}

	bool VulkanSwapChain::CreateImageViews() {
		imageViews.resize(images.size());
		for (size_t i = 0; i < images.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = images[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = imageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(device, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan swapchain image view.\n";
				return false;
			}
		}
		return true;
	}

	VulkanSwapChain::~VulkanSwapChain() {
		Reset();
	}

	bool VulkanSwapChain::Initialize(const VulkanDevice& sourceDevice, GLFWwindow* window) {
		device = sourceDevice.device;
		Support support;
		if (!QuerySupport(sourceDevice, support)) {
			std::cerr << "Failed to query Vulkan swapchain surface support.\n";
			return false;
		}
		const auto& [capabilities, formats, presentModes] = support;
		if (formats.empty() || presentModes.empty()) {
			std::cerr << "Failed to find Vulkan swapchain surface support.\n";
			return false;
		}
		const auto [format, colorSpace] = ChooseSurfaceFormat(formats);
		const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
		const VkExtent2D selectedExtent = ChooseExtent(capabilities, window);
		if (selectedExtent.width == 0 || selectedExtent.height == 0) {
			std::cerr << "Invalid Vulkan swapchain extent.\n";
			return false;
		}
		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
			imageCount = capabilities.maxImageCount;
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = sourceDevice.surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = format;
		createInfo.imageColorSpace = colorSpace;
		createInfo.imageExtent = selectedExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		const uint32_t queueFamilyIndices[] = {
			sourceDevice.queueFamilies.graphicsFamily,
			sourceDevice.queueFamilies.presentFamily
		};
		if (sourceDevice.queueFamilies.graphicsFamily != sourceDevice.queueFamilies.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.preTransform = capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;
		if (vkCreateSwapchainKHR(sourceDevice.device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan swapchain.\n";
			return false;
		}
		if (vkGetSwapchainImagesKHR(sourceDevice.device, swapChain, &imageCount, nullptr) != VK_SUCCESS)
			return false;
		images.resize(imageCount);
		if (vkGetSwapchainImagesKHR(sourceDevice.device, swapChain, &imageCount, images.data()) != VK_SUCCESS)
			return false;
		images.resize(imageCount);
		imageFormat = format;
		extent = selectedExtent;
		return CreateImageViews();
	}

	bool VulkanSwapChain::Recreate(const VulkanDevice& sourceDevice, GLFWwindow* window) {
		Reset();
		return Initialize(sourceDevice, window);
	}

	void VulkanSwapChain::Reset() {
		if (device == VK_NULL_HANDLE)
			return;
		for (const VkImageView imageView : imageViews)
			vkDestroyImageView(device, imageView, nullptr);
		imageViews.clear();
		images.clear();
		if (swapChain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(device, swapChain, nullptr);
			swapChain = VK_NULL_HANDLE;
		}
		imageFormat = VK_FORMAT_UNDEFINED;
		extent = {};
		device = VK_NULL_HANDLE;
	}
}
