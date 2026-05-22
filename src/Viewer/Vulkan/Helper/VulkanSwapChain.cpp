#include "VulkanSwapChain.h"

#include <algorithm>
#include <iostream>

namespace Chrivent {
	VulkanSwapChain::~VulkanSwapChain() {
		Destroy();
	}

	bool VulkanSwapChain::Initialize(const VulkanDeviceInfo& deviceInfo, GLFWwindow* window) {
		device = deviceInfo.device;
		const auto [capabilities,
			formats,
			presentModes] = QuerySupport(deviceInfo);
		if (formats.empty() || presentModes.empty()) {
			std::cerr << "Failed to find Vulkan swapchain surface support.\n";
			return false;
		}
		const auto [format, colorSpace] = ChooseSurfaceFormat(formats);
		const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
		const VkExtent2D extent = ChooseExtent(capabilities, window);
		if (extent.width == 0 || extent.height == 0) {
			std::cerr << "Invalid Vulkan swapchain extent.\n";
			return false;
		}
		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
			imageCount = capabilities.maxImageCount;
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = deviceInfo.surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = format;
		createInfo.imageColorSpace = colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		const uint32_t queueFamilyIndices[] = {
			deviceInfo.queueFamilies.graphicsFamily,
			deviceInfo.queueFamilies.presentFamily
		};
		if (deviceInfo.queueFamilies.graphicsFamily != deviceInfo.queueFamilies.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}
		createInfo.preTransform = capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;
		if (vkCreateSwapchainKHR(deviceInfo.device, &createInfo, nullptr, &info.swapChain) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan swapchain.\n";
			return false;
		}
		vkGetSwapchainImagesKHR(deviceInfo.device, info.swapChain, &imageCount, nullptr);
		info.images.resize(imageCount);
		vkGetSwapchainImagesKHR(deviceInfo.device, info.swapChain, &imageCount, info.images.data());
		info.imageFormat = format;
		info.extent = extent;
		return CreateImageViews();
	}

	bool VulkanSwapChain::Recreate(const VulkanDeviceInfo& deviceInfo, GLFWwindow* window) {
		if (deviceInfo.device != VK_NULL_HANDLE)
			vkDeviceWaitIdle(deviceInfo.device);
		Destroy();
		return Initialize(deviceInfo, window);
	}

	void VulkanSwapChain::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		for (const VkImageView imageView : info.imageViews)
			vkDestroyImageView(device, imageView, nullptr);
		info.imageViews.clear();
		info.images.clear();
		if (info.swapChain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(device, info.swapChain, nullptr);
			info.swapChain = VK_NULL_HANDLE;
		}
		info.imageFormat = VK_FORMAT_UNDEFINED;
		info.extent = {};
		device = VK_NULL_HANDLE;
	}

	VulkanSwapChainSupport VulkanSwapChain::QuerySupport(const VulkanDeviceInfo& deviceInfo) {
		VulkanSwapChainSupport support;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(deviceInfo.physicalDevice, deviceInfo.surface, &support.capabilities);
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(deviceInfo.physicalDevice, deviceInfo.surface, &formatCount, nullptr);
		if (formatCount > 0) {
			support.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(deviceInfo.physicalDevice, deviceInfo.surface, &formatCount, support.formats.data());
		}
		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(deviceInfo.physicalDevice, deviceInfo.surface, &presentModeCount, nullptr);
		if (presentModeCount > 0) {
			support.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(deviceInfo.physicalDevice, deviceInfo.surface, &presentModeCount, support.presentModes.data());
		}
		return support;
	}

	VkSurfaceFormatKHR VulkanSwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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
			static_cast<uint32_t>((std::max)(0, width)),
			static_cast<uint32_t>((std::max)(0, height))
		};
		extent.width = (std::clamp)(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = (std::clamp)(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return extent;
	}

	bool VulkanSwapChain::CreateImageViews() {
		info.imageViews.resize(info.images.size());
		for (size_t i = 0; i < info.images.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = info.images[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = info.imageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(device, &createInfo, nullptr, &info.imageViews[i]) != VK_SUCCESS) {
				std::cerr << "Failed to create Vulkan swapchain image view.\n";
				return false;
			}
		}
		return true;
	}
}
