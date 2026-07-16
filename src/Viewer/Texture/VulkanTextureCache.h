#pragma once

#include "Viewer/Texture/TextureCache.h"
#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Texture/VulkanTextureUploadContext.h"

#include <filesystem>

namespace Chrivent {
	// 캐시 정보와 Vulkan image, memory, view 및 sampler를 보관한다.
	struct VulkanTexture {
		TextureKey key;
		bool hasAlpha = false;
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory imageMemory = VK_NULL_HANDLE;
		VkImageView imageView = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	// 이미지 파일을 Vulkan texture로 업로드하고 공통 키로 재사용한다.
	class VulkanTextureCache : public TextureCache<VulkanTexture> {
		VkDevice device = VK_NULL_HANDLE;
		VulkanTextureUploadContext uploadContext;

		// 이미지 레이아웃 전환 명령을 기록한다.
		static void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
		// staging buffer에서 Vulkan image로 픽셀 데이터를 복사한다.
		static void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		// RGBA 픽셀 데이터를 Vulkan texture로 업로드한다.
		bool UploadRgbaPixels(const VulkanDevice& sourceDevice, VkCommandPool commandPool, const unsigned char* pixels, uint32_t width, uint32_t height, VulkanTexture& texture, bool clamp);
		// Vulkan image와 전용 메모리를 생성한다.
		static bool CreateImage(const VulkanDevice& sourceDevice, uint32_t width, uint32_t height, VkImage& image, VkDeviceMemory& imageMemory);
		// shader resource로 사용할 image view를 생성한다.
		bool CreateImageView(VkImage image, VkImageView& imageView) const;
		// shader sampler를 생성한다.
		bool CreateSampler(VkSampler& sampler, bool clamp) const;
		// 단일 텍스처의 Vulkan 리소스를 해제한다.
		void ResetTexture(VulkanTexture& texture) const;

	public:
		~VulkanTextureCache();

		// 텍스처를 캐시에서 찾거나 파일에서 로드해 Vulkan 텍스처로 반환한다.
		VulkanTexture Load(const VulkanDevice& sourceDevice, VkCommandPool commandPool, const std::filesystem::path& texturePath, bool clamp = false);
		// 텍스처가 없는 재질에 사용할 1x1 흰색 Vulkan 텍스처를 생성한다.
		VulkanTexture CreateWhiteTexture(const VulkanDevice& sourceDevice, VkCommandPool commandPool);
	};
}
