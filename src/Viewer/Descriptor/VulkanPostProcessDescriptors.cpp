#include "Viewer/Descriptor/VulkanPostProcessDescriptors.h"

#include "Viewer/Buffer/VulkanBuffer.h"

#include <limits>

namespace Chrivent {
	bool VulkanPostProcessDescriptors::CreateLayouts() {
		static constexpr VkDescriptorSetLayoutBinding frameDataBinding{
			.binding = SpirvBindingLayout::frameDataBinding,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		constexpr VkDescriptorSetLayoutCreateInfo frameLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &frameDataBinding
		};
		static constexpr VkDescriptorSetLayoutBinding parameterDataBinding{
			.binding = SpirvBindingLayout::parameterDataBinding,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		constexpr VkDescriptorSetLayoutCreateInfo parameterLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &parameterDataBinding
		};
		if (vkCreateDescriptorSetLayout(device, &frameLayoutInfo, nullptr,
			&descriptorSetLayouts[SpirvBindingLayout::frameDataSet]) != VK_SUCCESS
			|| vkCreateDescriptorSetLayout(device, &parameterLayoutInfo, nullptr,
				&descriptorSetLayouts[SpirvBindingLayout::parameterDataSet]) != VK_SUCCESS)
			return false;
		std::vector<VkDescriptorSetLayoutBinding> textureBindings;
		textureBindings.reserve(PostProcessInputLayout::maxTextureCount + PostProcessInputLayout::samplerCount);
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			textureBindings.emplace_back(VkDescriptorSetLayoutBinding{
				.binding = SpirvBindingLayout::ResolveTextureBinding(slot),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			});
		}
		for (uint32_t slot = 0; slot < PostProcessInputLayout::samplerCount; slot++) {
			textureBindings.emplace_back(VkDescriptorSetLayoutBinding{
				.binding = SpirvBindingLayout::ResolveSamplerBinding(slot),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			});
		}
		const VkDescriptorSetLayoutCreateInfo textureLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(textureBindings.size()),
			.pBindings = textureBindings.data()
		};
		if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr,
			&descriptorSetLayouts[SpirvBindingLayout::textureSet]) != VK_SUCCESS)
			return false;
		const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = SpirvBindingLayout::setCount,
			.pSetLayouts = descriptorSetLayouts
		};
		return vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS;
	}

	bool VulkanPostProcessDescriptors::CreateSampler() {
		constexpr VkSamplerCreateInfo samplerInfo{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.maxLod = 1.0f
		};
		return vkCreateSampler(device, &samplerInfo, nullptr, &sampler) == VK_SUCCESS;
	}

	bool VulkanPostProcessDescriptors::CreateDescriptorSets() {
		constexpr size_t maximum = std::numeric_limits<uint32_t>::max();
		if (imageCount > maximum || passCount == 0 || passCount > maximum / imageCount)
			return false;
		const uint32_t frameSetCount = static_cast<uint32_t>(imageCount);
		const uint32_t textureSetCount = static_cast<uint32_t>(imageCount * passCount);
		if (textureSetCount > (maximum - frameSetCount) / 2
			|| textureSetCount > maximum / PostProcessInputLayout::maxTextureCount
			|| textureSetCount > maximum / PostProcessInputLayout::samplerCount)
			return false;
		const uint32_t parameterSetCount = textureSetCount;
		const VkDescriptorPoolSize poolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameSetCount + parameterSetCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				textureSetCount * PostProcessInputLayout::maxTextureCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, textureSetCount * PostProcessInputLayout::samplerCount }
		};
		const VkDescriptorPoolCreateInfo poolInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = frameSetCount + parameterSetCount + textureSetCount,
			.poolSizeCount = 3,
			.pPoolSizes = poolSizes
		};
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			return false;
		std::vector layouts(frameSetCount + parameterSetCount + textureSetCount,
			descriptorSetLayouts[SpirvBindingLayout::textureSet]);
		for (uint32_t index = 0; index < frameSetCount; index++)
			layouts[index] = descriptorSetLayouts[SpirvBindingLayout::frameDataSet];
		for (uint32_t index = frameSetCount; index < frameSetCount + parameterSetCount; index++)
			layouts[index] = descriptorSetLayouts[SpirvBindingLayout::parameterDataSet];
		std::vector<VkDescriptorSet> sets(layouts.size());
		const VkDescriptorSetAllocateInfo allocateInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
			.pSetLayouts = layouts.data()
		};
		if (vkAllocateDescriptorSets(device, &allocateInfo, sets.data()) != VK_SUCCESS)
			return false;
		frameDataDescriptorSets.assign(sets.begin(), sets.begin() + frameSetCount);
		parameterDataDescriptorSets.assign(
			sets.begin() + frameSetCount, sets.begin() + frameSetCount + parameterSetCount);
		textureDescriptorSets.assign(sets.begin() + frameSetCount + parameterSetCount, sets.end());
		return true;
	}

	bool VulkanPostProcessDescriptors::BindBuffers(
		const std::span<const std::unique_ptr<VulkanBuffer>> frameDataBuffers,
		const std::span<const std::unique_ptr<VulkanBuffer>> parameterDataBuffers,
		const VkDeviceSize frameDataSize, const VkDeviceSize parameterDataSize) const {
		if (frameDataBuffers.size() != frameDataDescriptorSets.size()
			|| parameterDataBuffers.size() != parameterDataDescriptorSets.size())
			return false;
		for (size_t index = 0; index < frameDataBuffers.size(); index++) {
			if (!frameDataBuffers[index] || frameDataBuffers[index]->buffer == VK_NULL_HANDLE)
				return false;
			const VkDescriptorBufferInfo bufferInfo{
				.buffer = frameDataBuffers[index]->buffer,
				.range = frameDataSize
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frameDataDescriptorSets[index],
				.dstBinding = SpirvBindingLayout::frameDataBinding,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			};
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
		for (size_t index = 0; index < parameterDataBuffers.size(); index++) {
			if (!parameterDataBuffers[index] || parameterDataBuffers[index]->buffer == VK_NULL_HANDLE)
				return false;
			const VkDescriptorBufferInfo bufferInfo{
				.buffer = parameterDataBuffers[index]->buffer,
				.range = parameterDataSize
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = parameterDataDescriptorSets[index],
				.dstBinding = SpirvBindingLayout::parameterDataBinding,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			};
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
		return true;
	}

	VulkanPostProcessDescriptors::~VulkanPostProcessDescriptors() {
		Reset();
	}

	VkDescriptorSet VulkanPostProcessDescriptors::GetFrameDataDescriptorSet(const uint32_t imageIndex) const {
		return imageIndex < frameDataDescriptorSets.size()
			? frameDataDescriptorSets[imageIndex] : VK_NULL_HANDLE;
	}

	VkDescriptorSet VulkanPostProcessDescriptors::GetParameterDataDescriptorSet(
		const uint32_t imageIndex, const size_t passIndex) const {
		if (imageIndex >= imageCount || passIndex >= passCount)
			return VK_NULL_HANDLE;
		const size_t index = imageIndex * passCount + passIndex;
		return index < parameterDataDescriptorSets.size()
			? parameterDataDescriptorSets[index] : VK_NULL_HANDLE;
	}

	VkDescriptorSet VulkanPostProcessDescriptors::GetTextureDescriptorSet(
		const uint32_t imageIndex, const size_t passIndex) const {
		if (imageIndex >= imageCount || passIndex >= passCount)
			return VK_NULL_HANDLE;
		const size_t index = imageIndex * passCount + passIndex;
		return index < textureDescriptorSets.size()
			? textureDescriptorSets[index] : VK_NULL_HANDLE;
	}

	bool VulkanPostProcessDescriptors::IsCompatible(
		const size_t sourceImageCount, const size_t sourcePassCount) const {
		return pipelineLayout != VK_NULL_HANDLE && imageCount == sourceImageCount && passCount == sourcePassCount
			&& frameDataDescriptorSets.size() == imageCount
			&& parameterDataDescriptorSets.size() == imageCount * passCount
			&& textureDescriptorSets.size() == imageCount * passCount;
	}

	bool VulkanPostProcessDescriptors::Initialize(const VkDevice sourceDevice,
		const size_t sourceImageCount, const size_t sourcePassCount,
		const std::span<const std::unique_ptr<VulkanBuffer>> frameDataBuffers,
		const std::span<const std::unique_ptr<VulkanBuffer>> parameterDataBuffers,
		const VkDeviceSize frameDataSize, const VkDeviceSize parameterDataSize) {
		Reset();
		if (sourceDevice == VK_NULL_HANDLE || sourceImageCount == 0 || sourcePassCount == 0)
			return false;
		device = sourceDevice;
		imageCount = sourceImageCount;
		passCount = sourcePassCount;
		if (CreateLayouts() && CreateSampler() && CreateDescriptorSets()
			&& BindBuffers(frameDataBuffers, parameterDataBuffers, frameDataSize, parameterDataSize))
			return true;
		Reset();
		return false;
	}

	bool VulkanPostProcessDescriptors::UpdateTextures(const uint32_t imageIndex, const size_t passIndex,
		const std::span<const VkImageView> imageViews) const {
		const VkDescriptorSet descriptorSet = GetTextureDescriptorSet(imageIndex, passIndex);
		if (descriptorSet == VK_NULL_HANDLE || imageViews.size() != PostProcessInputLayout::maxTextureCount)
			return false;
		for (const VkImageView imageView : imageViews) {
			if (imageView == VK_NULL_HANDLE)
				return false;
		}
		std::vector<VkDescriptorImageInfo> imageInfos(
			PostProcessInputLayout::maxTextureCount + PostProcessInputLayout::samplerCount);
		std::vector<VkWriteDescriptorSet> writes;
		writes.reserve(imageInfos.size());
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			imageInfos[slot] = {
				.imageView = imageViews[slot],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			writes.emplace_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSet,
				.dstBinding = SpirvBindingLayout::ResolveTextureBinding(slot),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.pImageInfo = &imageInfos[slot]
			});
		}
		for (uint32_t slot = 0; slot < PostProcessInputLayout::samplerCount; slot++) {
			const size_t infoIndex = PostProcessInputLayout::maxTextureCount + slot;
			imageInfos[infoIndex] = { .sampler = sampler };
			writes.emplace_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSet,
				.dstBinding = SpirvBindingLayout::ResolveSamplerBinding(slot),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.pImageInfo = &imageInfos[infoIndex]
			});
		}
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		return true;
	}

	void VulkanPostProcessDescriptors::Reset() {
		if (device != VK_NULL_HANDLE) {
			if (pipelineLayout != VK_NULL_HANDLE)
				vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			if (descriptorPool != VK_NULL_HANDLE)
				vkDestroyDescriptorPool(device, descriptorPool, nullptr);
			if (sampler != VK_NULL_HANDLE)
				vkDestroySampler(device, sampler, nullptr);
			for (const VkDescriptorSetLayout layout : descriptorSetLayouts) {
				if (layout != VK_NULL_HANDLE)
					vkDestroyDescriptorSetLayout(device, layout, nullptr);
			}
		}
		frameDataDescriptorSets.clear();
		parameterDataDescriptorSets.clear();
		textureDescriptorSets.clear();
		for (VkDescriptorSetLayout& layout : descriptorSetLayouts)
			layout = VK_NULL_HANDLE;
		descriptorPool = VK_NULL_HANDLE;
		pipelineLayout = VK_NULL_HANDLE;
		sampler = VK_NULL_HANDLE;
		imageCount = 0;
		passCount = 0;
		device = VK_NULL_HANDLE;
	}

	void VulkanPostProcessDescriptors::Swap(VulkanPostProcessDescriptors& other) noexcept {
		std::swap(device, other.device);
		for (size_t index = 0; index < SpirvBindingLayout::setCount; index++)
			std::swap(descriptorSetLayouts[index], other.descriptorSetLayouts[index]);
		std::swap(descriptorPool, other.descriptorPool);
		std::swap(pipelineLayout, other.pipelineLayout);
		std::swap(sampler, other.sampler);
		frameDataDescriptorSets.swap(other.frameDataDescriptorSets);
		parameterDataDescriptorSets.swap(other.parameterDataDescriptorSets);
		textureDescriptorSets.swap(other.textureDescriptorSets);
		std::swap(imageCount, other.imageCount);
		std::swap(passCount, other.passCount);
	}
}
