#include "Viewer/Command/VulkanCommandContext.h"

namespace Chrivent {
	VulkanCommandContext::~VulkanCommandContext() {
		Reset();
	}

	GraphicsError::Result<void> VulkanCommandContext::Initialize(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain) {
		Reset();
		device = sourceDevice.GetDevice();
		if (device == VK_NULL_HANDLE) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "command context 초기화",
				"Vulkan device를 사용할 수 없습니다"));
		}
		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = sourceDevice.GetGraphicsQueueFamily();
		const VkResult result = vkCreateCommandPool(device, &createInfo, nullptr, &commandPool);
		if (result != VK_SUCCESS) {
			Reset();
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "command pool 생성",
				"Vulkan command pool을 만들지 못했습니다", result, true));
		}
		const auto commandBufferResult = commandBuffer.Initialize(
			sourceDevice, commandPool, sourceSwapChain);
		if (commandBufferResult)
			return {};
		const GraphicsError error = commandBufferResult.error();
		Reset();
		return std::unexpected(error);
	}

	void VulkanCommandContext::Reset() {
		commandBuffer.Reset();
		if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(device, commandPool, nullptr);
			commandPool = VK_NULL_HANDLE;
		}
		device = VK_NULL_HANDLE;
	}
}
