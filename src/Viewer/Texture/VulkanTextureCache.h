#pragma once

#include "Viewer/Command/VulkanUploadContext.h"
#include "Viewer/Texture/TextureCache.h"
#include "Viewer/Device/VulkanDevice.h"

#include <filesystem>
#include <optional>

namespace Chrivent {
	// 캐시 정보와 Vulkan image, memory, view 및 공유 sampler 참조를 보관한다.
	struct VulkanTexture {
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
		VkSampler wrapSampler = VK_NULL_HANDLE;
		VkSampler clampSampler = VK_NULL_HANDLE;
		VulkanUploadContext& uploadContext;

		// 이미지 레이아웃 전환 명령을 기록한다.
		static void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
		// staging buffer에서 Vulkan image로 픽셀 데이터를 복사한다.
		static void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		// RGBA 픽셀 데이터를 Vulkan texture로 업로드한다.
		GraphicsResult<void> UploadRgbaPixels(const VulkanDevice& sourceDevice, const unsigned char* pixels,
			uint32_t width, uint32_t height, VulkanTexture& texture, bool clamp);
		// Vulkan image와 전용 메모리를 생성한다.
		static GraphicsResult<void> CreateImage(const VulkanDevice& sourceDevice, uint32_t width,
			uint32_t height, VkImage& image, VkDeviceMemory& imageMemory);
		// shader resource로 사용할 image view를 생성한다.
		GraphicsResult<void> CreateImageView(VkImage image, VkImageView& imageView) const;
		// shader sampler를 생성한다.
		GraphicsResult<void> CreateSampler(VkSampler& sampler, bool clamp) const;
		// 텍스처들이 공유하는 wrap 및 clamp sampler를 준비한다.
		GraphicsResult<void> CreateSamplers();
		// 단일 텍스처의 Vulkan 리소스를 해제한다.
		void ResetTexture(VulkanTexture& texture) const;
		// 캐시가 공유하는 sampler들을 해제한다.
		void ResetSamplers();

	public:
		explicit VulkanTextureCache(VulkanUploadContext& sourceUploadContext) :
			uploadContext(sourceUploadContext) {}
		~VulkanTextureCache();

		// 텍스처를 캐시에서 찾거나 파일에서 로드해 Vulkan 텍스처로 반환한다.
		GraphicsResult<std::optional<VulkanTexture>> Load(const VulkanDevice& sourceDevice,
			const std::filesystem::path& texturePath, bool clamp = false);
		// 텍스처가 없는 재질에 사용할 1x1 흰색 Vulkan 텍스처를 생성한다.
		GraphicsResult<VulkanTexture> CreateWhiteTexture(const VulkanDevice& sourceDevice);
	};
}
