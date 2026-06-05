#pragma once

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Chrivent {
	struct VulkanQueueFamilyIndices {
		uint32_t graphicsFamily = 0;
		uint32_t presentFamily = 0;
		bool hasGraphicsFamily = false;
		bool hasPresentFamily = false;

		bool IsComplete() const { return hasGraphicsFamily && hasPresentFamily; }
	};

	struct VulkanDeviceInfo {
		VkInstance vkInstance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties properties{};
		VkDevice device = VK_NULL_HANDLE;
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		VkQueue presentQueue = VK_NULL_HANDLE;
		VkSampleCountFlagBits msaaSampleCount = VK_SAMPLE_COUNT_1_BIT;
		VulkanQueueFamilyIndices queueFamilies;
	};

	class VulkanDevice {
		static constexpr const char* kDeviceExtensions[] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		VulkanDeviceInfo info;

		// Vulkan 인스턴스를 생성한다.
		bool CreateInstance();
		// GLFW 윈도우에서 Vulkan surface를 생성한다.
		bool CreateSurface(GLFWwindow* window);
		// 렌더링에 사용할 물리 디바이스를 선택한다.
		bool PickPhysicalDevice();
		// 선택한 물리 디바이스에서 논리 디바이스와 큐를 생성한다.
		bool CreateLogicalDevice();
		// 물리 디바이스가 필요한 큐와 확장을 지원하는지 확인한다.
		bool IsDeviceSuitable(VkPhysicalDevice candidate) const;
		// 물리 디바이스에서 그래픽/표시 큐 패밀리를 찾는다.
		VulkanQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice candidate) const;
		// 물리 디바이스가 지원하는 샘플 수 중 현재 렌더러에서 사용할 값을 고른다.
		static VkSampleCountFlagBits ChooseMsaaSampleCount(VkPhysicalDevice candidate);
		// 물리 디바이스가 필수 디바이스 확장을 지원하는지 확인한다.
		static bool CheckDeviceExtensionSupport(VkPhysicalDevice candidate);

	public:
		VulkanDevice() = default;
		~VulkanDevice();

		VulkanDevice(const VulkanDevice&) = delete;
		VulkanDevice& operator=(const VulkanDevice&) = delete;
		VulkanDevice(VulkanDevice&&) = delete;
		VulkanDevice& operator=(VulkanDevice&&) = delete;
		
		const VulkanDeviceInfo& GetInfo() const { return info; }

		// Vulkan 디바이스 생성에 필요한 기본 리소스를 초기화한다.
		bool Initialize(GLFWwindow* window);
		// 생성한 Vulkan 디바이스 리소스를 해제한다.
		void Destroy();
	};
}
