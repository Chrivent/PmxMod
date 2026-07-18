#include "Viewer/Texture/VulkanTextureCache.h"

#include "Viewer/Buffer/VulkanBuffer.h"
#include "Viewer/Memory/VulkanMemory.h"

#include <iostream>
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

	bool VulkanTextureCache::UploadRgbaPixels(const VulkanDevice& sourceDevice, const unsigned char* pixels,
		const uint32_t width, const uint32_t height, VulkanTexture& texture, const bool clamp) {
		if (!CreateSamplers())
			return false;
		const VkDeviceSize imageSize = width * height * 4;
		VulkanBuffer stagingBuffer;
		if (!stagingBuffer.Initialize(sourceDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			return false;
		if (!stagingBuffer.Write(pixels, imageSize))
			return false;
		texture.width = width;
		texture.height = height;
		if (!CreateImage(sourceDevice, texture.width, texture.height, texture.image, texture.imageMemory)) {
			ResetTexture(texture);
			return false;
		}
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		if (!uploadContext.Begin(sourceDevice, commandBuffer)) {
			ResetTexture(texture);
			return false;
		}
		TransitionImageLayout(commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(commandBuffer, stagingBuffer.buffer, texture.image, texture.width, texture.height);
		TransitionImageLayout(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		if (!uploadContext.SubmitAndWait(sourceDevice)) {
			ResetTexture(texture);
			return false;
		}
		if (!CreateImageView(texture.image, texture.imageView)) {
			ResetTexture(texture);
			return false;
		}
		texture.sampler = clamp ? clampSampler : wrapSampler;
		return true;
	}

	bool VulkanTextureCache::CreateImage(const VulkanDevice& sourceDevice, const uint32_t width, const uint32_t height,
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
		if (vkCreateImage(sourceDevice.GetDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
			std::cerr << "Vulkan texture image를 만들지 못했습니다.\n";
			return false;
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(sourceDevice.GetDevice(), image, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(sourceDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
			std::cerr << "Vulkan texture image memory type을 찾지 못했습니다.\n";
			return false;
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(sourceDevice.GetDevice(), &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			std::cerr << "Vulkan texture image memory를 할당하지 못했습니다.\n";
			return false;
		}
		if (vkBindImageMemory(sourceDevice.GetDevice(), image, imageMemory, 0) != VK_SUCCESS) {
			std::cerr << "Vulkan texture image memory를 연결하지 못했습니다.\n";
			return false;
		}
		return true;
	}

	bool VulkanTextureCache::CreateImageView(const VkImage image, VkImageView& imageView) const {
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
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			std::cerr << "Vulkan texture image view를 만들지 못했습니다.\n";
			return false;
		}
		return true;
	}

	bool VulkanTextureCache::CreateSampler(VkSampler& sampler, const bool clamp) const {
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
		if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
			std::cerr << "Vulkan texture sampler를 만들지 못했습니다.\n";
			return false;
		}
		return true;
	}

	bool VulkanTextureCache::CreateSamplers() {
		if (device == VK_NULL_HANDLE)
			return false;
		if (wrapSampler != VK_NULL_HANDLE && clampSampler != VK_NULL_HANDLE)
			return true;
		ResetSamplers();
		if (!CreateSampler(wrapSampler, false))
			return false;
		if (CreateSampler(clampSampler, true))
			return true;
		ResetSamplers();
		return false;
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
		for (auto& texture : textures | std::views::values)
			ResetTexture(texture);
		textures.clear();
		ResetSamplers();
	}

	VulkanTexture VulkanTextureCache::Load(const VulkanDevice& sourceDevice,
		const std::filesystem::path& texturePath, const bool clamp) {
		device = sourceDevice.GetDevice();
		const TextureKey cacheKey{
			TextureKind::File,
			texturePath,
			clamp
		};
		if (const auto texture = FindCachedTexture(cacheKey))
			return *texture;
		const auto [pixels, width, height, components] = LoadImageRgba(texturePath);
		if (!pixels)
			return {};
		VulkanTexture texture;
		texture.hasAlpha = components == 4;
		if (!UploadRgbaPixels(sourceDevice, pixels.get(),
			width, height, texture, clamp))
			return {};
		textures.emplace(cacheKey, texture);
		return texture;
	}

	VulkanTexture VulkanTextureCache::CreateWhiteTexture(const VulkanDevice& sourceDevice) {
		device = sourceDevice.GetDevice();
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture(key))
			return *texture;
		constexpr unsigned char pixels[] = { 255, 255, 255, 255 };
		VulkanTexture texture;
		texture.hasAlpha = false;
		if (!UploadRgbaPixels(sourceDevice, pixels, 1, 1, texture, false))
			return {};
		textures.emplace(key, texture);
		return texture;
	}
}
