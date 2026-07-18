#pragma once

#include "Viewer/Device/VulkanDevice.h"

#include <vector>

namespace Chrivent {
	// Vulkan 후처리 pass가 사용하는 image, memory, view와 초기화 상태를 함께 관리한다.
	class VulkanPostProcessTarget {
		VkDevice device = VK_NULL_HANDLE;
		std::vector<VkImage> images;
		std::vector<VkDeviceMemory> memories;
		std::vector<VkImageView> imageViews;
		std::vector<bool> initialized;
		std::vector<bool> pendingInitialized;
		bool initializationFramePending = false;

		// 지정한 형식과 용도의 색상 image 하나를 생성한다.
		GraphicsResult<void> CreateImage(const VulkanDevice& sourceDevice, VkExtent2D extent, VkFormat format,
			VkImageUsageFlags usage, size_t index);
		// 다른 타깃과 Vulkan 리소스 소유권을 교환한다.
		void Swap(VulkanPostProcessTarget& other) noexcept;

	public:
		VulkanPostProcessTarget() = default;
		~VulkanPostProcessTarget();
		
		VulkanPostProcessTarget(const VulkanPostProcessTarget&) = delete;
		VulkanPostProcessTarget& operator=(const VulkanPostProcessTarget&) = delete;
		VulkanPostProcessTarget(VulkanPostProcessTarget&& other) noexcept;
		VulkanPostProcessTarget& operator=(VulkanPostProcessTarget&& other) noexcept;

		size_t GetImageCount() const { return images.size(); }
		const std::vector<VkImage>& GetImages() const { return images; }
		VkImage TryGetImage(const size_t index) const {
			return index < images.size() ? images[index] : VK_NULL_HANDLE;
		}
		VkImageView TryGetImageView(const size_t index) const {
			return index < imageViews.size() ? imageViews[index] : VK_NULL_HANDLE;
		}
		bool IsInitialized(const size_t index) const {
			const auto& states = initializationFramePending ? pendingInitialized : initialized;
			return index < states.size() && states[index];
		}

		// 지정한 개수의 후처리 색상 타깃을 생성한다.
		GraphicsResult<void> Initialize(const VulkanDevice& sourceDevice, size_t imageCount, VkExtent2D extent,
			VkFormat format, VkImageUsageFlags usage, bool trackInitialization);
		// 새 프레임의 이미지 상태 변경을 제출 전 임시 상태에 기록한다.
		void BeginInitializationFrame();
		// 지정한 타깃을 shader-read 가능한 상태로 사용한 이력이 있음을 기록한다.
		void MarkInitialized(size_t index);
		// 제출된 프레임의 임시 이미지 상태를 확정한다.
		void CommitInitializationFrame();
		// 제출되지 않은 프레임의 임시 이미지 상태를 버린다.
		void DiscardInitializationFrame();
		// 생성한 Vulkan image 리소스를 안전한 순서로 해제한다.
		void Reset();
	};
}
