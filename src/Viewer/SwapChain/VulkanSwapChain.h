#pragma once

#include "Viewer/Device/VulkanDevice.h"

#include <vector>

namespace Chrivent {
	// Vulkan surface가 지원하는 스왑체인 형식과 표시 모드를 보관한다.
	struct VulkanSwapChainSupport {
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	// Vulkan 스왑체인과 화면 image 및 image view를 관리한다.
	class VulkanSwapChain {
		VkDevice device = VK_NULL_HANDLE;

		// 물리 디바이스와 surface의 스왑체인 지원 정보를 조회한다.
		static bool QuerySupport(const VulkanDevice& sourceDevice, VulkanSwapChainSupport& support);
		// 사용할 surface format을 선택한다.
		static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
		// 사용할 present mode를 선택한다.
		static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
		// 현재 윈도우 크기에 맞는 swap extent를 선택한다.
		static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
		// 스왑체인 이미지를 렌더 타깃으로 쓰기 위한 image view를 생성한다.
		bool CreateImageViews();

	public:
		VkSwapchainKHR swapChain = VK_NULL_HANDLE;
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
		VkFormat imageFormat = VK_FORMAT_UNDEFINED;
		VkExtent2D extent{};

		VulkanSwapChain() = default;
		~VulkanSwapChain();

		VulkanSwapChain(const VulkanSwapChain&) = delete;
		VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;
		
		// Vulkan surface에 연결된 스왑체인과 image view를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, GLFWwindow* window);
		// 창 크기 변경에 맞춰 스왑체인과 image view를 다시 생성한다.
		bool Recreate(const VulkanDevice& sourceDevice, GLFWwindow* window);
		// 생성한 스왑체인 리소스를 해제한다.
		void Reset();
	};
}
