#include "VulkanDescriptorSet.h"

#include "../VulkanViewer.h"

#include <iostream>

namespace Chrivent {
	VulkanDescriptorSet::~VulkanDescriptorSet() {
		Destroy();
	}

	bool VulkanDescriptorSet::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VulkanPipelineInfo& pipelineInfo,
		const VulkanBufferInfo& vertexConstantBuffer,
		const VulkanBufferInfo& pixelConstantBuffer,
		std::vector<VulkanMaterial>& materials) {
		Destroy();
		device = deviceInfo.device;
		if (!CreateDescriptorPool(materials.size()))
			return false;
		if (!AllocateDescriptorSets(pipelineInfo, materials.size()))
			return false;
		UpdateDescriptorSets(vertexConstantBuffer, pixelConstantBuffer);
		UpdateTextureDescriptorSets(materials);
		return true;
	}

	void VulkanDescriptorSet::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		if (info.descriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device, info.descriptorPool, nullptr);
			info.descriptorPool = VK_NULL_HANDLE;
		}
		info.descriptorSets = {};
		info.textureDescriptorSets.clear();
		device = VK_NULL_HANDLE;
	}

	bool VulkanDescriptorSet::CreateDescriptorPool(const size_t materialCount) {
		if (materialCount > std::numeric_limits<uint32_t>::max()) {
			std::cerr << "Failed to create Vulkan descriptor pool: material count is too large.\n";
			return false;
		}
		std::vector poolSizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 2
			}
		};
		if (materialCount > 0) {
			poolSizes.emplace_back(VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = static_cast<uint32_t>(materialCount * 3)
			});
		}
		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = poolSizes.size();
		createInfo.pPoolSizes = poolSizes.data();
		createInfo.maxSets = info.descriptorSets.size() + materialCount;
		if (vkCreateDescriptorPool(device, &createInfo, nullptr, &info.descriptorPool) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan descriptor pool.\n";
			return false;
		}
		return true;
	}

	bool VulkanDescriptorSet::AllocateDescriptorSets(const VulkanPipelineInfo& pipelineInfo, const size_t materialCount) {
		const VkDescriptorSetLayout layouts[] = {
			pipelineInfo.descriptorSetLayouts[0],
			pipelineInfo.descriptorSetLayouts[1]
		};
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = info.descriptorPool;
		allocateInfo.descriptorSetCount = info.descriptorSets.size();
		allocateInfo.pSetLayouts = layouts;
		if (vkAllocateDescriptorSets(device, &allocateInfo, info.descriptorSets.data()) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan descriptor sets.\n";
			return false;
		}
		info.textureDescriptorSets.resize(materialCount);
		if (info.textureDescriptorSets.empty())
			return true;
		const std::vector textureLayouts(materialCount, pipelineInfo.descriptorSetLayouts[2]);
		VkDescriptorSetAllocateInfo textureAllocateInfo{};
		textureAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		textureAllocateInfo.descriptorPool = info.descriptorPool;
		textureAllocateInfo.descriptorSetCount = textureLayouts.size();
		textureAllocateInfo.pSetLayouts = textureLayouts.data();
		if (vkAllocateDescriptorSets(device, &textureAllocateInfo, info.textureDescriptorSets.data()) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan texture descriptor sets.\n";
			return false;
		}
		return true;
	}

	void VulkanDescriptorSet::UpdateDescriptorSets(
		const VulkanBufferInfo& vertexConstantBuffer,
		const VulkanBufferInfo& pixelConstantBuffer) const {
		const VkDescriptorBufferInfo vertexBufferInfo{
			.buffer = vertexConstantBuffer.buffer,
			.offset = 0,
			.range = vertexConstantBuffer.size
		};
		const VkDescriptorBufferInfo pixelBufferInfo{
			.buffer = pixelConstantBuffer.buffer,
			.offset = 0,
			.range = pixelConstantBuffer.size
		};
		const VkWriteDescriptorSet writes[] = {
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = info.descriptorSets[0],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &vertexBufferInfo,
				.pTexelBufferView = nullptr
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = info.descriptorSets[1],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &pixelBufferInfo,
				.pTexelBufferView = nullptr
			}
		};
		vkUpdateDescriptorSets(device, std::size(writes), writes, 0, nullptr);
	}

	void VulkanDescriptorSet::UpdateTextureDescriptorSets(std::vector<VulkanMaterial>& materials) const {
		for (size_t i = 0; i < materials.size(); i++) {
			if (i >= info.textureDescriptorSets.size())
				return;
			VulkanMaterial& material = materials[i];
			material.textureDescriptorSet = info.textureDescriptorSets[i];
			const VkDescriptorImageInfo imageInfos[] = {
				VkDescriptorImageInfo{
					.sampler = material.texture.sampler,
					.imageView = material.texture.imageView,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				},
				VkDescriptorImageInfo{
					.sampler = material.toonTexture.sampler,
					.imageView = material.toonTexture.imageView,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				},
				VkDescriptorImageInfo{
					.sampler = material.sphereTexture.sampler,
					.imageView = material.sphereTexture.imageView,
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				}
			};
			const VkWriteDescriptorSet writes[] = {
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.pNext = nullptr,
					.dstSet = material.textureDescriptorSet,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = &imageInfos[0],
					.pBufferInfo = nullptr,
					.pTexelBufferView = nullptr
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.pNext = nullptr,
					.dstSet = material.textureDescriptorSet,
					.dstBinding = 1,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = &imageInfos[1],
					.pBufferInfo = nullptr,
					.pTexelBufferView = nullptr
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.pNext = nullptr,
					.dstSet = material.textureDescriptorSet,
					.dstBinding = 2,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = &imageInfos[2],
					.pBufferInfo = nullptr,
					.pTexelBufferView = nullptr
				}
			};
			vkUpdateDescriptorSets(device, std::size(writes), writes, 0, nullptr);
		}
	}
}
