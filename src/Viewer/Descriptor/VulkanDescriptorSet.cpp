#include "Viewer/Descriptor/VulkanDescriptorSet.h"

#include "Viewer/DrawResource/VulkanModelResources.h"

#include <iostream>

namespace Chrivent {
	bool VulkanDescriptorSet::CreateDescriptorPool(const size_t materialCount) {
		constexpr size_t maximumDescriptorCount = std::numeric_limits<uint32_t>::max();
		if (materialCount > maximumDescriptorCount
			|| (passType == VulkanPassType::Model && materialCount > maximumDescriptorCount / 3)) {
			std::cerr << "Failed to create Vulkan descriptor pool: material count is too large.\n";
			return false;
		}
		std::vector poolSizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				.descriptorCount = 2
			}
		};
		if (passType == VulkanPassType::Model && materialCount > 0) {
			poolSizes.emplace_back(VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = static_cast<uint32_t>(materialCount * 3)
			});
			poolSizes.emplace_back(VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = static_cast<uint32_t>(materialCount * 3)
			});
		}
		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = poolSizes.size();
		createInfo.pPoolSizes = poolSizes.data();
		createInfo.maxSets = static_cast<uint32_t>(
			passType == VulkanPassType::Model ? 2 + materialCount : 2);
		if (vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan descriptor pool.\n";
			return false;
		}
		return true;
	}

	bool VulkanDescriptorSet::AllocateDescriptorSets(const VulkanPipeline& sourcePipeline, const size_t materialCount) {
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = 1;
		const VkDescriptorSetLayout vertexLayout = sourcePipeline.GetVertexDescriptorSetLayout();
		allocateInfo.pSetLayouts = &vertexLayout;
		if (vkAllocateDescriptorSets(device, &allocateInfo, &vertexDescriptorSet) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan vertex descriptor set.\n";
			return false;
		}
		const VkDescriptorSetLayout pixelLayout = sourcePipeline.GetPixelDescriptorSetLayout();
		allocateInfo.pSetLayouts = &pixelLayout;
		if (vkAllocateDescriptorSets(device, &allocateInfo, &pixelDescriptorSet) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan pixel descriptor set.\n";
			return false;
		}
		if (passType != VulkanPassType::Model)
			return true;
		textureDescriptorSets.resize(materialCount);
		if (textureDescriptorSets.empty())
			return true;
		const std::vector textureLayouts(materialCount, sourcePipeline.GetTextureDescriptorSetLayout());
		VkDescriptorSetAllocateInfo textureAllocateInfo{};
		textureAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		textureAllocateInfo.descriptorPool = descriptorPool;
		textureAllocateInfo.descriptorSetCount = textureLayouts.size();
		textureAllocateInfo.pSetLayouts = textureLayouts.data();
		if (vkAllocateDescriptorSets(device, &textureAllocateInfo, textureDescriptorSets.data()) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan texture descriptor sets.\n";
			return false;
		}
		return true;
	}

	void VulkanDescriptorSet::UpdateVertexDescriptorSet(const VulkanBuffer& vertexConstantBuffer, const VkDeviceSize vertexConstantRange) const {
		const VkDescriptorBufferInfo vertexBufferInfo{
			.buffer = vertexConstantBuffer.buffer,
			.offset = 0,
			.range = vertexConstantRange
		};
		const VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = vertexDescriptorSet,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.pImageInfo = nullptr,
			.pBufferInfo = &vertexBufferInfo,
			.pTexelBufferView = nullptr
		};
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	void VulkanDescriptorSet::UpdatePixelDescriptorSet(const VulkanBuffer& pixelConstantBuffer,
		const VkDeviceSize pixelConstantRange) const {
		const VkDescriptorBufferInfo pixelBufferInfo{
			.buffer = pixelConstantBuffer.buffer,
			.offset = 0,
			.range = pixelConstantRange
		};
		const VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = pixelDescriptorSet,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.pImageInfo = nullptr,
			.pBufferInfo = &pixelBufferInfo,
			.pTexelBufferView = nullptr
		};
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	void VulkanDescriptorSet::UpdateTextureDescriptorSets(std::vector<VulkanModelMaterial>& materials) const {
		for (size_t i = 0; i < materials.size(); i++) {
			if (i >= textureDescriptorSets.size())
				return;
			VulkanModelMaterial& material = materials[i];
			material.textureDescriptorSet = textureDescriptorSets[i];
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
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
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
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
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
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
					.pImageInfo = &imageInfos[2],
					.pBufferInfo = nullptr,
					.pTexelBufferView = nullptr
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = material.textureDescriptorSet,
					.dstBinding = 4,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &imageInfos[0]
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = material.textureDescriptorSet,
					.dstBinding = 5,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &imageInfos[1]
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = material.textureDescriptorSet,
					.dstBinding = 6,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &imageInfos[2]
				}
			};
			vkUpdateDescriptorSets(device, std::size(writes), writes, 0, nullptr);
		}
	}

	VulkanDescriptorSet::~VulkanDescriptorSet() {
		Reset();
	}

	bool VulkanDescriptorSet::Initialize(
		const VulkanDevice& sourceDevice,
		const VulkanPipeline& sourcePipeline,
		const VulkanBuffer& vertexConstantBuffer,
		const VkDeviceSize vertexConstantRange,
		const VulkanBuffer& pixelConstantBuffer,
		const VkDeviceSize pixelConstantRange,
		std::vector<VulkanModelMaterial>& materials,
		const VulkanPassType sourcePassType) {
		Reset();
		device = sourceDevice.device;
		passType = sourcePassType;
		if (!CreateDescriptorPool(materials.size()))
			return false;
		if (!AllocateDescriptorSets(sourcePipeline, materials.size()))
			return false;
		UpdateVertexDescriptorSet(vertexConstantBuffer, vertexConstantRange);
		UpdatePixelDescriptorSet(pixelConstantBuffer, pixelConstantRange);
		UpdateTextureDescriptorSets(materials);
		return true;
	}

	void VulkanDescriptorSet::Reset() {
		if (device == VK_NULL_HANDLE)
			return;
		if (descriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device, descriptorPool, nullptr);
			descriptorPool = VK_NULL_HANDLE;
		}
		vertexDescriptorSet = VK_NULL_HANDLE;
		pixelDescriptorSet = VK_NULL_HANDLE;
		textureDescriptorSets.clear();
		passType = VulkanPassType::Model;
		device = VK_NULL_HANDLE;
	}
}
