#pragma once

#include "Viewer/Command/VulkanUploadContext.h"
#include "Viewer/Texture/TextureCache.h"
#include "Viewer/Device/VulkanDevice.h"

#include <filesystem>
#include <optional>
#include <vector>

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
		VkCommandBuffer uploadCommandBuffer = VK_NULL_HANDLE;
		std::vector<TextureKey> pendingTextureKeys;
		bool uploadBatchActive = false;

		// 이미지 레이아웃 전환 명령을 기록한다.
		static void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
		// staging buffer에서 Vulkan image로 픽셀 데이터를 복사한다.
		static void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		// RGBA 픽셀 데이터를 Vulkan texture로 업로드한다.
		GraphicsError::Result<void> UploadRgbaPixels(const VulkanDevice& sourceDevice, const unsigned char* pixels,
			uint32_t width, uint32_t height, VulkanTexture& texture, bool clamp);
		// Vulkan image와 전용 메모리를 생성한다.
		static GraphicsError::Result<void> CreateImage(const VulkanDevice& sourceDevice, uint32_t width,
			uint32_t height, VkImage& image, VkDeviceMemory& imageMemory);
		// shader resource로 사용할 image view를 생성한다.
		GraphicsError::Result<void> CreateImageView(VkImage image, VkImageView& imageView) const;
		// shader sampler를 생성한다.
		GraphicsError::Result<void> CreateSampler(VkSampler& sampler, bool clamp) const;
		// 텍스처들이 공유하는 wrap 및 clamp sampler를 준비한다.
		GraphicsError::Result<void> CreateSamplers();
		// 단일 텍스처의 Vulkan 리소스를 해제한다.
		void ResetTexture(VulkanTexture& texture) const;
		// 캐시가 공유하는 sampler들을 해제한다.
		void ResetSamplers();
		// 제출하지 못한 batch에서 추가한 texture 리소스와 cache 항목을 제거한다.
		void RollbackUploadBatch();

	public:
		explicit VulkanTextureCache(VulkanUploadContext& sourceUploadContext) :
			uploadContext(sourceUploadContext) {}
		~VulkanTextureCache();

		// 여러 texture 업로드를 한 command buffer로 기록할 batch를 시작한다.
		GraphicsError::Result<void> BeginUploadBatch(const VulkanDevice& sourceDevice);
		// 기록한 texture upload batch를 한 번 제출한다.
		GraphicsError::Result<void> SubmitUploadBatch(const VulkanDevice& sourceDevice);
		// 제출하지 않은 texture upload batch와 새 cache 항목을 폐기한다.
		void CancelUploadBatch();
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 Vulkan 텍스처로 반환한다.
		GraphicsError::Result<std::optional<VulkanTexture>> Load(const VulkanDevice& sourceDevice,
			const std::filesystem::path& texturePath, bool clamp = false);
		// 텍스처가 없는 재질에 사용할 1x1 흰색 Vulkan 텍스처를 생성한다.
		GraphicsError::Result<VulkanTexture> CreateWhiteTexture(const VulkanDevice& sourceDevice);
	};
}
