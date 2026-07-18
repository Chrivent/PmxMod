#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Error/GraphicsError.h"

#include <vector>

namespace Chrivent {
	// Vulkan 스왑체인 작업이 정상 진행되었는지 재생성이 필요한지 구분한다.
	enum class VulkanSwapChainState {
		Ready,
		RecreateRequired
	};

	// Vulkan 스왑체인과 화면 image 및 image view를 관리한다.
	class VulkanSwapChain {
		// Vulkan surface가 지원하는 스왑체인 형식과 표시 모드를 보관한다.
		struct Support {
			VkSurfaceCapabilitiesKHR capabilities{};
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		VkDevice device = VK_NULL_HANDLE;
		VkSwapchainKHR swapChain = VK_NULL_HANDLE;
		std::vector<VkImage> images;
		std::vector<VkImageView> imageViews;
		VkFormat imageFormat = VK_FORMAT_UNDEFINED;
		VkExtent2D extent{};

		// 물리 디바이스와 surface의 스왑체인 지원 정보를 조회한다.
		static GraphicsResult<void> QuerySupport(const VulkanDevice& sourceDevice,
			Support& support);
		// 사용할 surface format을 선택한다.
		static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
		// 사용할 present mode를 선택한다.
		static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& presentModes);
		// 현재 윈도우 크기에 맞는 swap extent를 선택한다.
		static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
		// 스왑체인 이미지를 렌더 타깃으로 쓰기 위한 image view를 생성한다.
		GraphicsResult<void> CreateImageViews();

	public:
		VulkanSwapChain() = default;
		~VulkanSwapChain();

		VulkanSwapChain(const VulkanSwapChain&) = delete;
		VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;

		VkSwapchainKHR GetSwapChain() const { return swapChain; }
		size_t GetImageCount() const { return images.size(); }
		VkImage GetImage(const size_t imageIndex) const { return images[imageIndex]; }
		VkImageView GetImageView(const size_t imageIndex) const { return imageViews[imageIndex]; }
		VkFormat GetImageFormat() const { return imageFormat; }
		VkExtent2D GetExtent() const { return extent; }
		
		// Vulkan surface에 연결된 스왑체인과 image view를 생성한다.
		GraphicsResult<void> Initialize(const VulkanDevice& sourceDevice, GLFWwindow* window);
		// 창 크기 변경에 맞춰 스왑체인과 image view를 다시 생성한다.
		GraphicsResult<void> Recreate(const VulkanDevice& sourceDevice, GLFWwindow* window);
		// 다음 렌더링 대상 이미지를 획득하고 스왑체인 재생성 필요 여부를 반환한다.
		GraphicsResult<VulkanSwapChainState> AcquireNextImage(
			VkSemaphore imageAvailableSemaphore, uint32_t& imageIndex) const;
		// 렌더링이 끝난 이미지를 표시하고 스왑체인 재생성 필요 여부를 반환한다.
		GraphicsResult<VulkanSwapChainState> Present(
			VkQueue presentQueue, VkSemaphore renderFinishedSemaphore, uint32_t imageIndex) const;
		// 생성한 스왑체인 리소스를 해제한다.
		void Reset();
	};
}
