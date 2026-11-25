#include "Viewer/Descriptor/VulkanPostProcessDescriptors.h"

#include "Viewer/Buffer/VulkanBuffer.h"

#include <limits>

namespace Chrivent {
	GraphicsError::Result<void> VulkanPostProcessDescriptors::CreateLayouts() {
		static constexpr VkDescriptorSetLayoutBinding frameDataBinding{
			SpirvBindingLayout::frameDataBinding,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT
		};
		constexpr VkDescriptorSetLayoutCreateInfo frameLayoutInfo{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			1,
			&frameDataBinding
		};
		static constexpr VkDescriptorSetLayoutBinding parameterDataBinding{
			SpirvBindingLayout::parameterDataBinding,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			1,
			VK_SHADER_STAGE_FRAGMENT_BIT
		};
		constexpr VkDescriptorSetLayoutCreateInfo parameterLayoutInfo{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			1,
			&parameterDataBinding
		};
		VkResult result = vkCreateDescriptorSetLayout(device, &frameLayoutInfo, nullptr,
			&descriptorSetLayouts[SpirvBindingLayout::frameDataSet]);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "frame descriptor layout 생성",
				"Vulkan frame descriptor set layout을 만들지 못했습니다", result, true));
		}
		result = vkCreateDescriptorSetLayout(device, &parameterLayoutInfo, nullptr,
			&descriptorSetLayouts[SpirvBindingLayout::parameterDataSet]);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "parameter descriptor layout 생성",
				"Vulkan parameter descriptor set layout을 만들지 못했습니다", result, true));
		}
		std::vector<VkDescriptorSetLayoutBinding> textureBindings;
		textureBindings.reserve(PostProcessInputLayout::maxTextureCount + PostProcessInputLayout::samplerCount);
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			textureBindings.emplace_back(VkDescriptorSetLayoutBinding{
				SpirvBindingLayout::ResolveTextureBinding(slot),
				VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				1,
				VK_SHADER_STAGE_FRAGMENT_BIT
			});
		}
		for (uint32_t slot = 0; slot < PostProcessInputLayout::samplerCount; slot++) {
			textureBindings.emplace_back(VkDescriptorSetLayoutBinding{
				SpirvBindingLayout::ResolveSamplerBinding(slot),
				VK_DESCRIPTOR_TYPE_SAMPLER,
				1,
				VK_SHADER_STAGE_FRAGMENT_BIT
			});
		}
		const VkDescriptorSetLayoutCreateInfo textureLayoutInfo{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			static_cast<uint32_t>(textureBindings.size()),
			textureBindings.data()
		};
		result = vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr,
			&descriptorSetLayouts[SpirvBindingLayout::textureSet]);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture descriptor layout 생성",
				"Vulkan texture descriptor set layout을 만들지 못했습니다", result, true));
		}
		const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			SpirvBindingLayout::setCount,
			descriptorSetLayouts
		};
		result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 pipeline layout 생성",
				"Vulkan 후처리 pipeline layout을 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanPostProcessDescriptors::CreateSampler() {
		constexpr auto samplerInfo = [] {
			VkSamplerCreateInfo value{};
			value.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			value.magFilter = VK_FILTER_LINEAR;
			value.minFilter = VK_FILTER_LINEAR;
			value.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			value.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			value.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			value.maxLod = 1.0f;
			return value;
		}();
		const VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 sampler 생성",
				"Vulkan 후처리 sampler를 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanPostProcessDescriptors::CreateDescriptorSets() {
		constexpr size_t maximum = std::numeric_limits<uint32_t>::max();
		if (imageCount > maximum || passCount == 0 || passCount > maximum / imageCount) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "후처리 descriptor 개수 계산",
				"Vulkan 후처리 이미지 또는 패스 수가 descriptor 한도를 넘습니다"));
		}
		const uint32_t frameSetCount = static_cast<uint32_t>(imageCount);
		const uint32_t textureSetCount = static_cast<uint32_t>(imageCount * passCount);
		if (textureSetCount > (maximum - frameSetCount) / 2
			|| textureSetCount > maximum / PostProcessInputLayout::maxTextureCount
			|| textureSetCount > maximum / PostProcessInputLayout::samplerCount) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "후처리 descriptor 개수 계산",
				"Vulkan 후처리 texture 또는 sampler descriptor 수가 한도를 넘습니다"));
		}
		const uint32_t parameterSetCount = textureSetCount;
		const VkDescriptorPoolSize poolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameSetCount + parameterSetCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				textureSetCount * PostProcessInputLayout::maxTextureCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, textureSetCount * PostProcessInputLayout::samplerCount }
		};
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = frameSetCount + parameterSetCount + textureSetCount;
		poolInfo.poolSizeCount = 3;
		poolInfo.pPoolSizes = poolSizes;
		VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 descriptor pool 생성",
				"Vulkan 후처리 descriptor pool을 만들지 못했습니다", result, true));
		}
		std::vector layouts(frameSetCount + parameterSetCount + textureSetCount,
			descriptorSetLayouts[SpirvBindingLayout::textureSet]);
		for (uint32_t index = 0; index < frameSetCount; index++)
			layouts[index] = descriptorSetLayouts[SpirvBindingLayout::frameDataSet];
		for (uint32_t index = frameSetCount; index < frameSetCount + parameterSetCount; index++)
			layouts[index] = descriptorSetLayouts[SpirvBindingLayout::parameterDataSet];
		std::vector<VkDescriptorSet> sets(layouts.size());
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
		allocateInfo.pSetLayouts = layouts.data();
		result = vkAllocateDescriptorSets(device, &allocateInfo, sets.data());
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "후처리 descriptor set 할당",
				"Vulkan 후처리 descriptor set을 할당하지 못했습니다", result, true));
		}
		frameDataDescriptorSets.assign(sets.begin(), sets.begin() + frameSetCount);
		parameterDataDescriptorSets.assign(
			sets.begin() + frameSetCount, sets.begin() + frameSetCount + parameterSetCount);
		textureDescriptorSets.assign(sets.begin() + frameSetCount + parameterSetCount, sets.end());
		textureImageViewCache.resize(
			textureDescriptorSets.size() * PostProcessInputLayout::maxTextureCount);
		return {};
	}

	GraphicsError::Result<void> VulkanPostProcessDescriptors::BindBuffers(
		const std::span<const std::unique_ptr<VulkanBuffer>> frameDataBuffers,
		const std::span<const std::unique_ptr<VulkanBuffer>> parameterDataBuffers,
		const VkDeviceSize frameDataSize, const VkDeviceSize parameterDataSize,
		const VkDeviceSize parameterDataStride) const {
		if (frameDataBuffers.size() != frameDataDescriptorSets.size()
			|| parameterDataBuffers.size() != imageCount
			|| parameterDataDescriptorSets.size() != imageCount * passCount) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "후처리 buffer descriptor 연결",
				"Vulkan 후처리 버퍼와 descriptor set 수가 일치하지 않습니다"));
		}
		for (size_t index = 0; index < frameDataBuffers.size(); index++) {
			if (!frameDataBuffers[index] || frameDataBuffers[index]->GetBuffer() == VK_NULL_HANDLE) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::InvalidState, "frame buffer descriptor 연결",
					"Vulkan 후처리 frame buffer를 사용할 수 없습니다"));
			}
			const VkDescriptorBufferInfo bufferInfo{ frameDataBuffers[index]->GetBuffer(), 0, frameDataSize };
			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = frameDataDescriptorSets[index];
			write.dstBinding = SpirvBindingLayout::frameDataBinding;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.pBufferInfo = &bufferInfo;
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
		for (size_t index = 0; index < parameterDataDescriptorSets.size(); index++) {
			const size_t imageIndex = index / passCount;
			const size_t passIndex = index % passCount;
			if (!parameterDataBuffers[imageIndex]
				|| parameterDataBuffers[imageIndex]->GetBuffer() == VK_NULL_HANDLE) {
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::InvalidState, "parameter buffer descriptor 연결",
					"Vulkan 후처리 parameter buffer를 사용할 수 없습니다"));
			}
			const VkDescriptorBufferInfo bufferInfo{
				parameterDataBuffers[imageIndex]->GetBuffer(),
				passIndex * parameterDataStride,
				parameterDataSize
			};
			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = parameterDataDescriptorSets[index];
			write.dstBinding = SpirvBindingLayout::parameterDataBinding;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.pBufferInfo = &bufferInfo;
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
		return {};
	}

	VulkanPostProcessDescriptors::~VulkanPostProcessDescriptors() {
		Reset();
	}

	VkDescriptorSet VulkanPostProcessDescriptors::TryGetFrameDataDescriptorSet(
		const uint32_t imageIndex) const {
		return imageIndex < frameDataDescriptorSets.size()
			? frameDataDescriptorSets[imageIndex] : VK_NULL_HANDLE;
	}

	VkDescriptorSet VulkanPostProcessDescriptors::TryGetParameterDataDescriptorSet(
		const uint32_t imageIndex, const size_t passIndex) const {
		if (imageIndex >= imageCount || passIndex >= passCount)
			return VK_NULL_HANDLE;
		const size_t index = imageIndex * passCount + passIndex;
		return index < parameterDataDescriptorSets.size()
			? parameterDataDescriptorSets[index] : VK_NULL_HANDLE;
	}

	VkDescriptorSet VulkanPostProcessDescriptors::TryGetTextureDescriptorSet(
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

	GraphicsError::Result<void> VulkanPostProcessDescriptors::Initialize(const VkDevice sourceDevice,
		const size_t sourceImageCount, const size_t sourcePassCount,
		const std::span<const std::unique_ptr<VulkanBuffer>> frameDataBuffers,
		const std::span<const std::unique_ptr<VulkanBuffer>> parameterDataBuffers,
		const VkDeviceSize frameDataSize, const VkDeviceSize parameterDataSize,
		const VkDeviceSize parameterDataStride) {
		Reset();
		if (sourceDevice == VK_NULL_HANDLE || sourceImageCount == 0 || sourcePassCount == 0) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "후처리 descriptor 초기화",
				"Vulkan device, 이미지 수 또는 패스 수가 올바르지 않습니다"));
		}
		device = sourceDevice;
		imageCount = sourceImageCount;
		passCount = sourcePassCount;
		auto result = CreateLayouts();
		if (result)
			result = CreateSampler();
		if (result)
			result = CreateDescriptorSets();
		if (result) {
			result = BindBuffers(frameDataBuffers, parameterDataBuffers,
				frameDataSize, parameterDataSize, parameterDataStride);
		}
		if (result)
			return {};
		const GraphicsError error = result.error();
		Reset();
		return std::unexpected(error);
	}

	bool VulkanPostProcessDescriptors::UpdateTextures(const uint32_t imageIndex, const size_t passIndex,
		const std::span<const VkImageView> imageViews) {
		const VkDescriptorSet descriptorSet = TryGetTextureDescriptorSet(imageIndex, passIndex);
		if (descriptorSet == VK_NULL_HANDLE || imageViews.size() != PostProcessInputLayout::maxTextureCount)
			return false;
		for (const VkImageView imageView : imageViews) {
			if (imageView == VK_NULL_HANDLE)
				return false;
		}
		const size_t setIndex = imageIndex * passCount + passIndex;
		const size_t cacheOffset = setIndex * PostProcessInputLayout::maxTextureCount;
		bool unchanged = true;
		for (size_t index = 0; index < imageViews.size(); index++) {
			if (textureImageViewCache[cacheOffset + index] != imageViews[index]) {
				unchanged = false;
				break;
			}
		}
		if (unchanged)
			return true;
		constexpr size_t descriptorCount =
			PostProcessInputLayout::maxTextureCount + PostProcessInputLayout::samplerCount;
		VkDescriptorImageInfo imageInfos[descriptorCount]{};
		VkWriteDescriptorSet writes[descriptorCount]{};
		size_t writeCount = 0;
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			imageInfos[slot] = { VK_NULL_HANDLE, imageViews[slot], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			auto& write = writes[writeCount++];
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = descriptorSet;
			write.dstBinding = SpirvBindingLayout::ResolveTextureBinding(slot);
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			write.pImageInfo = &imageInfos[slot];
		}
		for (uint32_t slot = 0; slot < PostProcessInputLayout::samplerCount; slot++) {
			const size_t infoIndex = PostProcessInputLayout::maxTextureCount + slot;
			imageInfos[infoIndex] = { sampler };
			auto& write = writes[writeCount++];
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = descriptorSet;
			write.dstBinding = SpirvBindingLayout::ResolveSamplerBinding(slot);
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			write.pImageInfo = &imageInfos[infoIndex];
		}
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeCount), writes, 0, nullptr);
		for (size_t index = 0; index < imageViews.size(); index++)
			textureImageViewCache[cacheOffset + index] = imageViews[index];
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
		textureImageViewCache.clear();
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
		textureImageViewCache.swap(other.textureImageViewCache);
		std::swap(imageCount, other.imageCount);
		std::swap(passCount, other.passCount);
	}
}
