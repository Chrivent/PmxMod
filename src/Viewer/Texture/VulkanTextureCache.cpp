#include "Viewer/Texture/VulkanTextureCache.h"

#include "Viewer/Buffer/VulkanBuffer.h"
#include <limits>
#include <memory>
#include <ranges>

namespace Chrivent {
	void VulkanTextureCache::TransitionImageLayout(const VkCommandBuffer commandBuffer, const VkImage image,
		const VkImageLayout oldLayout, const VkImageLayout newLayout) {
		VkImageMemoryBarrier2 barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		}
		const VkDependencyInfo dependencyInfo{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier
		};
		vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
	}

	void VulkanTextureCache::CopyBufferToImage(const VkCommandBuffer commandBuffer, const VkBuffer buffer,
		const VkImage image, const uint32_t width, const uint32_t height) {
		VkBufferImageCopy region;
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	GraphicsError::Result<void> VulkanTextureCache::UploadRgbaPixels(
		const VulkanDevice& sourceDevice, const unsigned char* pixels,
		const uint32_t width, const uint32_t height, VulkanTexture& texture, const bool clamp) {
		const auto samplerResult = CreateSamplers();
		if (!samplerResult)
			return std::unexpected(samplerResult.error());
		if (pixels == nullptr || width == 0 || height == 0
			|| static_cast<VkDeviceSize>(width) > std::numeric_limits<VkDeviceSize>::max()
				/ 4 / static_cast<VkDeviceSize>(height)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "texture 업로드",
				"Vulkan texture 픽셀 데이터 또는 크기가 올바르지 않습니다"));
		}
		const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;
		auto stagingBuffer = std::make_unique<VulkanBuffer>();
		const auto stagingResult = stagingBuffer->Initialize(sourceDevice, imageSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (!stagingResult)
			return std::unexpected(stagingResult.error());
		if (!stagingBuffer->Write(pixels, imageSize)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture staging buffer 기록",
				"Vulkan texture 픽셀을 staging buffer에 기록하지 못했습니다"));
		}
		texture.width = width;
		texture.height = height;
		const auto imageResult = CreateImage(
			sourceDevice, texture.width, texture.height, texture.image, texture.imageMemory);
		if (!imageResult) {
			ResetTexture(texture);
			return std::unexpected(imageResult.error());
		}
		const bool standaloneUpload = !uploadBatchActive;
		VkCommandBuffer commandBuffer = uploadCommandBuffer;
		if (standaloneUpload) {
			const auto beginResult = uploadContext.BeginBatch(sourceDevice, commandBuffer);
			if (!beginResult) {
				ResetTexture(texture);
				return std::unexpected(beginResult.error());
			}
		}
		TransitionImageLayout(commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(commandBuffer, stagingBuffer->GetBuffer(), texture.image, texture.width, texture.height);
		TransitionImageLayout(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		const auto retainResult = uploadContext.RetainStagingBuffer(std::move(stagingBuffer));
		if (!retainResult) {
			if (standaloneUpload)
				uploadContext.CancelBatch();
			else
				CancelUploadBatch();
			ResetTexture(texture);
			return std::unexpected(retainResult.error());
		}
		if (standaloneUpload) {
			const auto submitResult = uploadContext.SubmitBatch(sourceDevice);
			if (!submitResult) {
				ResetTexture(texture);
				return std::unexpected(submitResult.error());
			}
		}
		const auto viewResult = CreateImageView(texture.image, texture.imageView);
		if (!viewResult) {
			if (uploadBatchActive)
				CancelUploadBatch();
			ResetTexture(texture);
			return std::unexpected(viewResult.error());
		}
		texture.sampler = clamp ? clampSampler : wrapSampler;
		return {};
	}

	void VulkanTextureCache::RollbackUploadBatch() {
		for (const TextureKey& key : pendingTextureKeys) {
			const auto texture = textures.find(key);
			if (texture == textures.end())
				continue;
			ResetTexture(texture->second);
			textures.erase(texture);
		}
		pendingTextureKeys.clear();
	}

	GraphicsError::Result<void> VulkanTextureCache::BeginUploadBatch(const VulkanDevice& sourceDevice) {
		if (uploadBatchActive) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "texture upload batch 시작",
				"Vulkan texture upload batch가 이미 활성화되어 있습니다"));
		}
		device = sourceDevice.GetDevice();
		const auto beginResult = uploadContext.BeginBatch(sourceDevice, uploadCommandBuffer);
		if (!beginResult)
			return std::unexpected(beginResult.error());
		pendingTextureKeys.clear();
		uploadBatchActive = true;
		return {};
	}

	GraphicsError::Result<void> VulkanTextureCache::SubmitUploadBatch(const VulkanDevice& sourceDevice) {
		if (!uploadBatchActive) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "texture upload batch 제출",
				"제출할 Vulkan texture upload batch가 없습니다"));
		}
		uploadBatchActive = false;
		uploadCommandBuffer = VK_NULL_HANDLE;
		const auto submitResult = uploadContext.SubmitBatch(sourceDevice);
		if (!submitResult) {
			RollbackUploadBatch();
			return std::unexpected(submitResult.error());
		}
		pendingTextureKeys.clear();
		return {};
	}

	void VulkanTextureCache::CancelUploadBatch() {
		if (!uploadBatchActive)
			return;
		uploadContext.CancelBatch();
		uploadBatchActive = false;
		uploadCommandBuffer = VK_NULL_HANDLE;
		RollbackUploadBatch();
	}

	GraphicsError::Result<void> VulkanTextureCache::CreateImage(
		const VulkanDevice& sourceDevice, const uint32_t width, const uint32_t height,
		VkImage& image, VkDeviceMemory& imageMemory) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		VkResult result = vkCreateImage(sourceDevice.GetDevice(), &imageInfo, nullptr, &image);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture image 생성",
				"Vulkan texture image를 만들지 못했습니다", result, true));
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(sourceDevice.GetDevice(), image, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!sourceDevice.TryFindMemoryType(
			memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::UnsupportedFeature, "texture image memory type 선택",
				"Vulkan texture image memory type을 찾지 못했습니다"));
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		result = vkAllocateMemory(sourceDevice.GetDevice(), &allocateInfo, nullptr, &imageMemory);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture image memory 할당",
				"Vulkan texture image memory를 할당하지 못했습니다", result, true));
		}
		result = vkBindImageMemory(sourceDevice.GetDevice(), image, imageMemory, 0);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture image memory 연결",
				"Vulkan texture image memory를 연결하지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanTextureCache::CreateImageView(
		const VkImage image, VkImageView& imageView) const {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		const VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture image view 생성",
				"Vulkan texture image view를 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanTextureCache::CreateSampler(
		VkSampler& sampler, const bool clamp) const {
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		const VkSamplerAddressMode addressMode = clamp
			? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
			: VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeU = addressMode;
		samplerInfo.addressModeV = addressMode;
		samplerInfo.addressModeW = addressMode;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		const VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture sampler 생성",
				"Vulkan texture sampler를 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanTextureCache::CreateSamplers() {
		if (device == VK_NULL_HANDLE) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "texture sampler 생성",
				"Vulkan device를 사용할 수 없습니다"));
		}
		if (wrapSampler != VK_NULL_HANDLE && clampSampler != VK_NULL_HANDLE)
			return {};
		ResetSamplers();
		const auto wrapResult = CreateSampler(wrapSampler, false);
		if (!wrapResult)
			return std::unexpected(wrapResult.error());
		const auto clampResult = CreateSampler(clampSampler, true);
		if (clampResult)
			return {};
		ResetSamplers();
		return std::unexpected(clampResult.error());
	}

	void VulkanTextureCache::ResetTexture(VulkanTexture& texture) const {
		if (device == VK_NULL_HANDLE)
			return;
		texture.sampler = VK_NULL_HANDLE;
		if (texture.imageView != VK_NULL_HANDLE) {
			vkDestroyImageView(device, texture.imageView, nullptr);
			texture.imageView = VK_NULL_HANDLE;
		}
		if (texture.image != VK_NULL_HANDLE) {
			vkDestroyImage(device, texture.image, nullptr);
			texture.image = VK_NULL_HANDLE;
		}
		if (texture.imageMemory != VK_NULL_HANDLE) {
			vkFreeMemory(device, texture.imageMemory, nullptr);
			texture.imageMemory = VK_NULL_HANDLE;
		}
		texture.width = 0;
		texture.height = 0;
	}

	void VulkanTextureCache::ResetSamplers() {
		if (device != VK_NULL_HANDLE && clampSampler != VK_NULL_HANDLE)
			vkDestroySampler(device, clampSampler, nullptr);
		if (device != VK_NULL_HANDLE && wrapSampler != VK_NULL_HANDLE)
			vkDestroySampler(device, wrapSampler, nullptr);
		clampSampler = VK_NULL_HANDLE;
		wrapSampler = VK_NULL_HANDLE;
	}

	VulkanTextureCache::~VulkanTextureCache() {
		CancelUploadBatch();
		for (auto& texture : textures | std::views::values)
			ResetTexture(texture);
		textures.clear();
		ResetSamplers();
	}

	GraphicsError::Result<std::optional<VulkanTexture>> VulkanTextureCache::Load(const VulkanDevice& sourceDevice,
		const std::filesystem::path& texturePath, const bool clamp) {
		device = sourceDevice.GetDevice();
		const TextureKey cacheKey{
			TextureKind::File,
			texturePath,
			clamp
		};
		if (const auto texture = FindCachedTexture(cacheKey))
			return std::optional{ *texture };
		const auto [pixels, width, height, components] = LoadImageRgba(texturePath);
		if (!pixels)
			return std::optional<VulkanTexture>{};
		VulkanTexture texture;
		texture.hasAlpha = components == 4;
		const auto uploadResult = UploadRgbaPixels(
			sourceDevice, pixels.get(), width, height, texture, clamp);
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		textures.emplace(cacheKey, texture);
		if (uploadBatchActive)
			pendingTextureKeys.emplace_back(cacheKey);
		return std::optional{ texture };
	}

	GraphicsError::Result<VulkanTexture> VulkanTextureCache::CreateWhiteTexture(
		const VulkanDevice& sourceDevice) {
		device = sourceDevice.GetDevice();
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		constexpr unsigned char pixels[] = { 255, 255, 255, 255 };
		VulkanTexture texture;
		texture.hasAlpha = false;
		const auto uploadResult = UploadRgbaPixels(sourceDevice, pixels, 1, 1, texture, false);
		if (!uploadResult)
			return std::unexpected(uploadResult.error());
		textures.emplace(key, texture);
		if (uploadBatchActive)
			pendingTextureKeys.emplace_back(key);
		return texture;
	}
}
