#include "Viewer/SwapChain/VulkanSwapChain.h"

#include <algorithm>
#include <limits>

namespace Chrivent {
	GraphicsError::Result<void> VulkanSwapChain::QuerySupport(const VulkanDevice& sourceDevice,
		Support& support) {
		support = {};
		VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(sourceDevice.GetPhysicalDevice(),
			sourceDevice.GetSurface(), &support.capabilities);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InitializationFailed, "surface capability 조회",
				"Vulkan surface capability를 조회하지 못했습니다", result, true));
		}
		uint32_t formatCount = 0;
		result = vkGetPhysicalDeviceSurfaceFormatsKHR(sourceDevice.GetPhysicalDevice(),
			sourceDevice.GetSurface(), &formatCount, nullptr);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InitializationFailed, "surface format 조회",
				"Vulkan surface format 개수를 조회하지 못했습니다", result, true));
		}
		if (formatCount > 0) {
			support.formats.resize(formatCount);
			result = vkGetPhysicalDeviceSurfaceFormatsKHR(sourceDevice.GetPhysicalDevice(),
				sourceDevice.GetSurface(), &formatCount, support.formats.data());
			if (result != VK_SUCCESS) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::InitializationFailed, "surface format 조회",
					"Vulkan surface format을 조회하지 못했습니다", result, true));
			}
			support.formats.resize(formatCount);
		}
		uint32_t presentModeCount = 0;
		result = vkGetPhysicalDeviceSurfacePresentModesKHR(sourceDevice.GetPhysicalDevice(),
			sourceDevice.GetSurface(), &presentModeCount, nullptr);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InitializationFailed, "present mode 조회",
				"Vulkan present mode 개수를 조회하지 못했습니다", result, true));
		}
		if (presentModeCount > 0) {
			support.presentModes.resize(presentModeCount);
			result = vkGetPhysicalDeviceSurfacePresentModesKHR(sourceDevice.GetPhysicalDevice(),
				sourceDevice.GetSurface(), &presentModeCount, support.presentModes.data());
			if (result != VK_SUCCESS) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::InitializationFailed, "present mode 조회",
					"Vulkan present mode를 조회하지 못했습니다", result, true));
			}
			support.presentModes.resize(presentModeCount);
		}
		return {};
	}

	VkSurfaceFormatKHR VulkanSwapChain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM
				&& format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_R8G8B8A8_UNORM
				&& format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return format;
		}
		return formats.front();
	}

	VkPresentModeKHR VulkanSwapChain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes) {
		for (const VkPresentModeKHR mode : presentModes) {
			if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
				return mode;
		}
		for (const VkPresentModeKHR mode : presentModes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				return mode;
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanSwapChain::ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
		GLFWwindow* window) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D selectedExtent{
			static_cast<uint32_t>(std::max(0, width)),
			static_cast<uint32_t>(std::max(0, height))
		};
		selectedExtent.width = (std::clamp)(
			selectedExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		selectedExtent.height = (std::clamp)(
			selectedExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		return selectedExtent;
	}

	GraphicsError::Result<void> VulkanSwapChain::CreateImageViews() {
		imageViews.resize(images.size());
		for (size_t index = 0; index < images.size(); index++) {
			VkImageViewCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = images[index],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = imageFormat,
				.components = {
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};
			const VkResult result = vkCreateImageView(device, &createInfo,
				nullptr, &imageViews[index]);
			if (result != VK_SUCCESS) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::ResourceCreationFailed, "swap chain image view 생성",
					"Vulkan 스왑체인 image view를 만들지 못했습니다", result, true));
			}
		}
		return {};
	}

	VulkanSwapChain::~VulkanSwapChain() {
		Reset();
	}

	GraphicsError::Result<void> VulkanSwapChain::Initialize(const VulkanDevice& sourceDevice,
		GLFWwindow* window) {
		Reset();
		if (sourceDevice.GetDevice() == VK_NULL_HANDLE || window == nullptr) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "swap chain 생성",
				"Vulkan device 또는 출력 창을 사용할 수 없습니다"));
		}
		device = sourceDevice.GetDevice();
		Support support;
		const auto supportResult = QuerySupport(sourceDevice, support);
		if (!supportResult)
			return std::unexpected(supportResult.error());
		const auto& [capabilities, formats, presentModes] = support;
		if (formats.empty() || presentModes.empty()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::UnsupportedFeature, "swap chain surface 선택",
				"지원되는 Vulkan surface format 또는 present mode가 없습니다"));
		}
		const auto [format, colorSpace] = ChooseSurfaceFormat(formats);
		const VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);
		const VkExtent2D selectedExtent = ChooseExtent(capabilities, window);
		if (selectedExtent.width == 0 || selectedExtent.height == 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "swap chain extent 선택",
				"Vulkan 스왑체인 extent가 올바르지 않습니다"));
		}
		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
			imageCount = capabilities.maxImageCount;
		const uint32_t queueFamilyIndices[] = {
			sourceDevice.GetGraphicsQueueFamily(),
			sourceDevice.GetPresentQueueFamily()
		};
		VkSwapchainCreateInfoKHR createInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = sourceDevice.GetSurface(),
			.minImageCount = imageCount,
			.imageFormat = format,
			.imageColorSpace = colorSpace,
			.imageExtent = selectedExtent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = presentMode,
			.clipped = VK_TRUE
		};
		if (sourceDevice.GetGraphicsQueueFamily() != sourceDevice.GetPresentQueueFamily()) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkResult result = vkCreateSwapchainKHR(sourceDevice.GetDevice(),
			&createInfo, nullptr, &swapChain);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain 생성",
				"Vulkan 스왑체인을 만들지 못했습니다", result, true));
		}
		result = vkGetSwapchainImagesKHR(sourceDevice.GetDevice(),
			swapChain, &imageCount, nullptr);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain image 조회",
				"Vulkan 스왑체인 image 개수를 조회하지 못했습니다", result, true));
		}
		images.resize(imageCount);
		result = vkGetSwapchainImagesKHR(sourceDevice.GetDevice(),
			swapChain, &imageCount, images.data());
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "swap chain image 조회",
				"Vulkan 스왑체인 image를 조회하지 못했습니다", result, true));
		}
		images.resize(imageCount);
		imageFormat = format;
		extent = selectedExtent;
		return CreateImageViews();
	}

	GraphicsError::Result<void> VulkanSwapChain::Recreate(const VulkanDevice& sourceDevice,
		GLFWwindow* window) {
		return Initialize(sourceDevice, window);
	}

	GraphicsError::Result<VulkanSwapChainState> VulkanSwapChain::AcquireNextImage(
		const VkSemaphore imageAvailableSemaphore, uint32_t& imageIndex) const {
		if (device == VK_NULL_HANDLE || swapChain == VK_NULL_HANDLE
			|| imageAvailableSemaphore == VK_NULL_HANDLE) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "swap chain 이미지 획득",
				"Vulkan 스왑체인 또는 이미지 획득 세마포어를 사용할 수 없습니다"));
		}
		const VkResult result = vkAcquireNextImageKHR(device, swapChain,
			std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore,
			VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
			return VulkanSwapChainState::RecreateRequired;
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::PresentationFailed, "swap chain 이미지 획득",
				"Vulkan이 다음 스왑체인 이미지를 획득하지 못했습니다", result, true));
		}
		return VulkanSwapChainState::Ready;
	}

	GraphicsError::Result<VulkanSwapChainState> VulkanSwapChain::Present(
		const VkQueue presentQueue, const VkSemaphore renderFinishedSemaphore,
		const uint32_t imageIndex) const {
		if (swapChain == VK_NULL_HANDLE || presentQueue == VK_NULL_HANDLE
			|| renderFinishedSemaphore == VK_NULL_HANDLE || imageIndex >= images.size()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "swap chain present",
				"Vulkan present에 필요한 스왑체인, 큐 또는 세마포어를 사용할 수 없습니다"));
		}
		const VkSwapchainKHR swapChains[] = { swapChain };
		const VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &renderFinishedSemaphore,
			.swapchainCount = 1,
			.pSwapchains = swapChains,
			.pImageIndices = &imageIndex
		};
		const VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			return VulkanSwapChainState::RecreateRequired;
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::PresentationFailed, "swap chain present",
				"Vulkan 프레임을 표시하지 못했습니다", result, true));
		}
		return VulkanSwapChainState::Ready;
	}

	void VulkanSwapChain::Reset() {
		if (device != VK_NULL_HANDLE) {
			for (const VkImageView imageView : imageViews) {
				if (imageView != VK_NULL_HANDLE)
					vkDestroyImageView(device, imageView, nullptr);
			}
			if (swapChain != VK_NULL_HANDLE)
				vkDestroySwapchainKHR(device, swapChain, nullptr);
		}
		imageViews.clear();
		images.clear();
		swapChain = VK_NULL_HANDLE;
		imageFormat = VK_FORMAT_UNDEFINED;
		extent = {};
		device = VK_NULL_HANDLE;
	}
}
