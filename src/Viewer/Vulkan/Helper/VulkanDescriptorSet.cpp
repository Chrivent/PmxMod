#include "VulkanDescriptorSet.h"

#include <iostream>

namespace Chrivent {
	VulkanDescriptorSet::~VulkanDescriptorSet() {
		Destroy();
	}

	bool VulkanDescriptorSet::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VulkanPipelineInfo& pipelineInfo,
		const VulkanBufferInfo& vertexConstantBuffer,
		const VulkanBufferInfo& pixelConstantBuffer) {
		Destroy();
		device = deviceInfo.device;
		if (!CreateDescriptorPool())
			return false;
		if (!AllocateDescriptorSets(pipelineInfo))
			return false;
		UpdateDescriptorSets(vertexConstantBuffer, pixelConstantBuffer);
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
		device = VK_NULL_HANDLE;
	}

	bool VulkanDescriptorSet::CreateDescriptorPool() {
		constexpr VkDescriptorPoolSize poolSize{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 2
		};
		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = 1;
		createInfo.pPoolSizes = &poolSize;
		createInfo.maxSets = info.descriptorSets.size();
		if (vkCreateDescriptorPool(device, &createInfo, nullptr, &info.descriptorPool) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan descriptor pool.\n";
			return false;
		}
		return true;
	}

	bool VulkanDescriptorSet::AllocateDescriptorSets(const VulkanPipelineInfo& pipelineInfo) {
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
}
