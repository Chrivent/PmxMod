#pragma once

#include "Viewer/Device/GraphicsCapabilities.h"

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Chrivent {
	// 그래픽과 표시 작업에 사용할 Vulkan 큐 패밀리 인덱스를 보관한다.
	struct VulkanQueueFamilyIndices {
		uint32_t graphicsFamily = 0;
		uint32_t presentFamily = 0;
		bool hasGraphicsFamily = false;
		bool hasPresentFamily = false;

		bool IsComplete() const { return hasGraphicsFamily && hasPresentFamily; }
	};

	// Vulkan 물리·논리 디바이스와 그래픽 및 표시 큐를 관리한다.
	class VulkanDevice {
		static constexpr const char* kDeviceExtensions[] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

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
		// 물리 디바이스 종류와 성능 한도를 기준으로 선택 우선순위를 계산한다.
		static uint64_t ScorePhysicalDevice(const VkPhysicalDeviceProperties& properties);
		// Vulkan 물리 디바이스 종류를 로그용 이름으로 변환한다.
		static const char* ResolvePhysicalDeviceTypeName(VkPhysicalDeviceType type);
		// 물리 디바이스에서 그래픽/표시 큐 패밀리를 찾는다.
		VulkanQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice candidate) const;
		// 물리 디바이스가 지원하는 샘플 수 중 현재 렌더러에서 사용할 값을 고른다.
		static VkSampleCountFlagBits ChooseMsaaSampleCount(VkPhysicalDevice candidate);
		// 물리 디바이스가 필수 디바이스 확장을 지원하는지 확인한다.
		static bool CheckDeviceExtensionSupport(VkPhysicalDevice candidate);
		// Vulkan 버전 정수를 로그용 문자열로 변환한다.
		static std::string ResolveVersionName(uint32_t version);
		// 선택한 물리 디바이스의 지원 기능과 한도를 기록한다.
		void ResolveCapabilities();
		// 생성한 Vulkan 디바이스 전체 수명주기를 종료한다.
		void Shutdown();

	public:
		VkInstance vkInstance = VK_NULL_HANDLE;
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties properties{};
		VkDevice device = VK_NULL_HANDLE;
		VkQueue graphicsQueue = VK_NULL_HANDLE;
		VkQueue presentQueue = VK_NULL_HANDLE;
		VkSampleCountFlagBits msaaSampleCount = VK_SAMPLE_COUNT_1_BIT;
		VulkanQueueFamilyIndices queueFamilies;
		GraphicsCapabilities capabilities;
		uint32_t instanceApiVersion = VK_API_VERSION_1_0;

		VulkanDevice() = default;
		~VulkanDevice();

		VulkanDevice(const VulkanDevice&) = delete;
		VulkanDevice& operator=(const VulkanDevice&) = delete;
		
		// Vulkan 디바이스 생성에 필요한 기본 리소스를 초기화한다.
		bool Initialize(GLFWwindow* window);
	};
}
