#include "VulkanTextureCache.h"

#include "Helper/VulkanBuffer.h"
#include "Helper/VulkanMemory.h"
#include "../Viewer.h"

#include <iostream>
#include <ranges>
#include <stb_image.h>

namespace Chrivent {
	bool VulkanTextureCache::BeginSingleTimeCommands(const VulkanDevice& sourceDevice, const VkCommandPool commandPool,
		VkCommandBuffer& commandBuffer) {
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool = commandPool;
		allocateInfo.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(sourceDevice.device, &allocateInfo, &commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan texture command buffer.\n";
			return false;
		}
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			std::cerr << "Failed to begin Vulkan texture command buffer.\n";
			vkFreeCommandBuffers(sourceDevice.device, commandPool, 1, &commandBuffer);
			commandBuffer = VK_NULL_HANDLE;
			return false;
		}
		return true;
	}

	bool VulkanTextureCache::EndSingleTimeCommands(const VulkanDevice& sourceDevice, const VkCommandPool commandPool,
		const VkCommandBuffer commandBuffer) {
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			std::cerr << "Failed to record Vulkan texture command buffer.\n";
			vkFreeCommandBuffers(sourceDevice.device, commandPool, 1, &commandBuffer);
			return false;
		}
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		if (vkQueueSubmit(sourceDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
			std::cerr << "Failed to submit Vulkan texture command buffer.\n";
			vkFreeCommandBuffers(sourceDevice.device, commandPool, 1, &commandBuffer);
			return false;
		}
		vkQueueWaitIdle(sourceDevice.graphicsQueue);
		vkFreeCommandBuffers(sourceDevice.device, commandPool, 1, &commandBuffer);
		return true;
	}

	void VulkanTextureCache::TransitionImageLayout(const VkCommandBuffer commandBuffer, const VkImage image,
		const VkImageLayout oldLayout, const VkImageLayout newLayout) {
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
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
		VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
			0, nullptr, 0, nullptr,
			1, &barrier);
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

	bool VulkanTextureCache::UploadRgbaPixels(const VulkanDevice& sourceDevice, const VkCommandPool commandPool,
		const unsigned char* pixels, const uint32_t width, const uint32_t height, VulkanTexture& texture, const bool clamp) const {
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
			DestroyTexture(texture);
			return false;
		}
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		if (!BeginSingleTimeCommands(sourceDevice, commandPool, commandBuffer)) {
			DestroyTexture(texture);
			return false;
		}
		TransitionImageLayout(commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(commandBuffer, stagingBuffer.buffer, texture.image, texture.width, texture.height);
		TransitionImageLayout(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		if (!EndSingleTimeCommands(sourceDevice, commandPool, commandBuffer)) {
			DestroyTexture(texture);
			return false;
		}
		if (!CreateImageView(texture.image, texture.imageView)) {
			DestroyTexture(texture);
			return false;
		}
		if (!CreateSampler(texture.sampler, clamp)) {
			DestroyTexture(texture);
			return false;
		}
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
		if (vkCreateImage(sourceDevice.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan texture image.\n";
			return false;
		}
		VkMemoryRequirements memoryRequirements{};
		vkGetImageMemoryRequirements(sourceDevice.device, image, &memoryRequirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(sourceDevice, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
			std::cerr << "Failed to find Vulkan texture image memory type.\n";
			return false;
		}
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memoryRequirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(sourceDevice.device, &allocateInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan texture image memory.\n";
			return false;
		}
		if (vkBindImageMemory(sourceDevice.device, image, imageMemory, 0) != VK_SUCCESS) {
			std::cerr << "Failed to bind Vulkan texture image memory.\n";
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
			std::cerr << "Failed to create Vulkan texture image view.\n";
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
			std::cerr << "Failed to create Vulkan texture sampler.\n";
			return false;
		}
		return true;
	}

	void VulkanTextureCache::DestroyTexture(VulkanTexture& texture) const {
		if (device == VK_NULL_HANDLE)
			return;
		if (texture.sampler != VK_NULL_HANDLE) {
			vkDestroySampler(device, texture.sampler, nullptr);
			texture.sampler = VK_NULL_HANDLE;
		}
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

	VulkanTextureCache::~VulkanTextureCache() {
		for (const auto& texture : textures | std::views::values) {
			const auto vulkanTexture = std::static_pointer_cast<VulkanTexture>(texture);
			DestroyTexture(*vulkanTexture);
		}
		textures.clear();
	}

	VulkanTexture VulkanTextureCache::Load(const VulkanDevice& sourceDevice, const VkCommandPool commandPool,
		const std::filesystem::path& texturePath, const bool clamp) {
		device = sourceDevice.device;
		const TextureKey cacheKey{
			TextureKind::File,
			texturePath,
			clamp
		};
		if (const auto texture = FindCachedTexture<VulkanTexture>(cacheKey))
			return *texture;
		int x = 0, y = 0, comp = 0;
		stbi_uc* image = Viewer::LoadImageRgba(texturePath, x, y, comp);
		if (!image)
			return {};
		const bool textureHasAlpha = comp == 4;
		const auto texture = std::make_shared<VulkanTexture>();
		texture->key = cacheKey;
		texture->hasAlpha = textureHasAlpha;
		if (!UploadRgbaPixels(sourceDevice, commandPool, image, x, y, *texture, clamp)) {
			stbi_image_free(image);
			return {};
		}
		stbi_image_free(image);
		textures[cacheKey] = texture;
		return *texture;
	}

	VulkanTexture VulkanTextureCache::CreateWhiteTexture(const VulkanDevice& sourceDevice, const VkCommandPool commandPool) {
		device = sourceDevice.device;
		const TextureKey key{ TextureKind::White };
		if (const auto texture = FindCachedTexture<VulkanTexture>(key))
			return *texture;
		constexpr unsigned char pixels[] = { 255, 255, 255, 255 };
		const auto texture = std::make_shared<VulkanTexture>();
		texture->key = key;
		texture->hasAlpha = false;
		if (!UploadRgbaPixels(sourceDevice, commandPool, pixels, 1, 1, *texture, false))
			return {};
		textures[key] = texture;
		return *texture;
	}
}
