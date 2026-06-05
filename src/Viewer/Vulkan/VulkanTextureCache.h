#pragma once

#include "../TextureCache.h"
#include "Helper/VulkanDevice.h"

#include <filesystem>

namespace Chrivent {
	struct VulkanTexture : Texture {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkImageView imageView = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	class VulkanTextureCache : public TextureCache {
		VkDevice device = VK_NULL_HANDLE;

		// 일회성 복사/레이아웃 전환용 command buffer 기록을 시작한다.
		static bool BeginSingleTimeCommands(const VulkanDeviceInfo& deviceInfo, VkCommandPool commandPool, VkCommandBuffer& commandBuffer);
		// 일회성 command buffer를 제출하고 해제한다.
		static bool EndSingleTimeCommands(const VulkanDeviceInfo& deviceInfo, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
		// 이미지 레이아웃 전환 명령을 기록한다.
		static void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
		// staging buffer에서 Vulkan image로 픽셀 데이터를 복사한다.
		static void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		// RGBA 픽셀 데이터를 Vulkan texture로 업로드한다.
		bool UploadRgbaPixels(const VulkanDeviceInfo& deviceInfo, VkCommandPool commandPool, const unsigned char* pixels, uint32_t width, uint32_t height, VulkanTexture& texture, bool clamp) const;
		// Vulkan image와 전용 메모리를 생성한다.
		static bool CreateImage(const VulkanDeviceInfo& deviceInfo, uint32_t width, uint32_t height, VkImage& image, VkDeviceMemory& imageMemory);
		// shader resource로 사용할 image view를 생성한다.
		bool CreateImageView(VkImage image, VkImageView& imageView) const;
		// shader sampler를 생성한다.
		bool CreateSampler(VkSampler& sampler, bool clamp) const;
		// 단일 텍스처의 Vulkan 리소스를 해제한다.
		void DestroyTexture(VulkanTexture& texture) const;

	public:
		~VulkanTextureCache() override;

		// 텍스처를 캐시에서 찾거나 파일에서 로드해 Vulkan 텍스처로 반환한다.
		VulkanTexture Load(const VulkanDeviceInfo& deviceInfo, VkCommandPool commandPool, const std::filesystem::path& texturePath, bool clamp = false);
		// 텍스처가 없는 재질에 사용할 1x1 흰색 Vulkan 텍스처를 생성한다.
		VulkanTexture CreateWhiteTexture(const VulkanDeviceInfo& deviceInfo, VkCommandPool commandPool);
	};
}
