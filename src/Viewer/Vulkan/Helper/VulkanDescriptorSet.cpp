#include "VulkanDescriptorSet.h"

#include "../VulkanViewer.h"

#include <iostream>

namespace Chrivent {
	bool VulkanDescriptorSet::CreateDescriptorPool(const size_t materialCount) {
		if (materialCount > std::numeric_limits<uint32_t>::max()) {
			std::cerr << "Failed to create Vulkan descriptor pool: material count is too large.\n";
			return false;
		}
		std::vector poolSizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				.descriptorCount = static_cast<uint32_t>(1 + materialCount)
			}
		};
		if (passType == VulkanPassType::Model && materialCount > 0) {
			poolSizes.emplace_back(VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = static_cast<uint32_t>(materialCount * 3)
			});
		}
		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = poolSizes.size();
		createInfo.pPoolSizes = poolSizes.data();
		createInfo.maxSets = passType == VulkanPassType::Model
			? 1 + materialCount + materialCount
			: 1 + materialCount;
		if (vkCreateDescriptorPool(device, &createInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan descriptor pool.\n";
			return false;
		}
		return true;
	}

	bool VulkanDescriptorSet::AllocateDescriptorSets(const VulkanPipelineInfo& pipelineInfo, const size_t materialCount) {
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &pipelineInfo.descriptorSetLayouts[0];
		if (vkAllocateDescriptorSets(device, &allocateInfo, &info.vertexDescriptorSet) != VK_SUCCESS) {
			std::cerr << "Failed to allocate Vulkan vertex descriptor set.\n";
			return false;
		}
		info.pixelDescriptorSets.resize(materialCount);
		if (!info.pixelDescriptorSets.empty()) {
			const std::vector pixelLayouts(materialCount, pipelineInfo.descriptorSetLayouts[1]);
			VkDescriptorSetAllocateInfo pixelAllocateInfo{};
			pixelAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			pixelAllocateInfo.descriptorPool = descriptorPool;
			pixelAllocateInfo.descriptorSetCount = pixelLayouts.size();
			pixelAllocateInfo.pSetLayouts = pixelLayouts.data();
			if (vkAllocateDescriptorSets(device, &pixelAllocateInfo, info.pixelDescriptorSets.data()) != VK_SUCCESS) {
				std::cerr << "Failed to allocate Vulkan pixel descriptor sets.\n";
				return false;
			}
		}
		if (passType != VulkanPassType::Model)
			return true;
		textureDescriptorSets.resize(materialCount);
		if (textureDescriptorSets.empty())
			return true;
		const std::vector textureLayouts(materialCount, pipelineInfo.descriptorSetLayouts[2]);
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

	void VulkanDescriptorSet::UpdateVertexDescriptorSet(
		const VulkanBufferInfo& vertexConstantBuffer,
		const VkDeviceSize vertexConstantRange) const {
		const VkDescriptorBufferInfo vertexBufferInfo{
			.buffer = vertexConstantBuffer.buffer,
			.offset = 0,
			.range = vertexConstantRange
		};
		const VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = info.vertexDescriptorSet,
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

	void VulkanDescriptorSet::UpdatePixelDescriptorSets(
		const VulkanBufferInfo& pixelConstantBuffer,
		const VkDeviceSize pixelConstantRange,
		std::vector<VulkanMaterial>& materials) const {
		for (size_t i = 0; i < materials.size(); i++) {
			if (i >= info.pixelDescriptorSets.size())
				return;
			VulkanMaterial& material = materials[i];
			VkDescriptorSet* descriptorSet = nullptr;
			if (passType == VulkanPassType::Model) {
				descriptorSet = &material.pixelDescriptorSet;
			} else if (passType == VulkanPassType::Edge) {
				descriptorSet = &material.edgePixelDescriptorSet;
			} else if (passType == VulkanPassType::GroundShadow) {
				descriptorSet = &material.groundShadowPixelDescriptorSet;
			}
			if (descriptorSet == nullptr)
				continue;
			*descriptorSet = info.pixelDescriptorSets[i];
			const VkDescriptorBufferInfo pixelBufferInfo{
				.buffer = pixelConstantBuffer.buffer,
				.offset = 0,
				.range = pixelConstantRange
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = *descriptorSet,
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
	}

	void VulkanDescriptorSet::UpdateTextureDescriptorSets(std::vector<VulkanMaterial>& materials) const {
		for (size_t i = 0; i < materials.size(); i++) {
			if (i >= textureDescriptorSets.size())
				return;
			VulkanMaterial& material = materials[i];
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

	VulkanDescriptorSet::~VulkanDescriptorSet() {
		Destroy();
	}

	bool VulkanDescriptorSet::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VulkanPipelineInfo& pipelineInfo,
		const VulkanBufferInfo& vertexConstantBuffer,
		const VkDeviceSize vertexConstantRange,
		const VulkanBufferInfo& pixelConstantBuffer,
		const VkDeviceSize pixelConstantRange,
		std::vector<VulkanMaterial>& materials,
		const VulkanPassType sourcePassType) {
		Destroy();
		device = deviceInfo.device;
		passType = sourcePassType;
		if (!CreateDescriptorPool(materials.size()))
			return false;
		if (!AllocateDescriptorSets(pipelineInfo, materials.size()))
			return false;
		UpdateVertexDescriptorSet(vertexConstantBuffer, vertexConstantRange);
		UpdatePixelDescriptorSets(pixelConstantBuffer, pixelConstantRange, materials);
		UpdateTextureDescriptorSets(materials);
		return true;
	}

	void VulkanDescriptorSet::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		if (descriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device, descriptorPool, nullptr);
			descriptorPool = VK_NULL_HANDLE;
		}
		info.vertexDescriptorSet = VK_NULL_HANDLE;
		info.pixelDescriptorSets.clear();
		textureDescriptorSets.clear();
		passType = VulkanPassType::Model;
		device = VK_NULL_HANDLE;
	}
}
