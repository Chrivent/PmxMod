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

		// 지정한 형식과 용도의 색상 image 하나를 생성한다.
		bool CreateImage(const VulkanDevice& sourceDevice, VkExtent2D extent, VkFormat format,
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
		VkImage ResolveImage(size_t index) const {
			return index < images.size() ? images[index] : VK_NULL_HANDLE;
		}
		VkImageView ResolveImageView(size_t index) const {
			return index < imageViews.size() ? imageViews[index] : VK_NULL_HANDLE;
		}
		bool IsInitialized(size_t index) const {
			return index < initialized.size() && initialized[index];
		}

		// 지정한 개수의 후처리 색상 타깃을 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, size_t imageCount, VkExtent2D extent,
			VkFormat format, VkImageUsageFlags usage, bool trackInitialization);
		// 지정한 타깃을 shader-read 가능한 상태로 사용한 이력이 있음을 기록한다.
		void MarkInitialized(size_t index);
		// 생성한 Vulkan image 리소스를 안전한 순서로 해제한다.
		void Reset();
	};
}
