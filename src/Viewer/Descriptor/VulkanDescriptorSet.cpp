#include "Viewer/Descriptor/VulkanDescriptorSet.h"

#include "Viewer/DrawResource/VulkanModelResources.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

namespace Chrivent {
	GraphicsError::Result<void> VulkanDescriptorSet::CreateDescriptorPool(const size_t textureDescriptorCount) {
		constexpr size_t maximumDescriptorCount = std::numeric_limits<uint32_t>::max();
		if (textureDescriptorCount > maximumDescriptorCount / SceneShaderInputLayout::textureCount) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "descriptor pool 생성",
				"texture descriptor 개수가 Vulkan 범위를 벗어났습니다"));
		}
		std::vector poolSizes{
			VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 }
		};
		if (textureDescriptorCount > 0) {
			poolSizes.emplace_back(VkDescriptorPoolSize{
				VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				static_cast<uint32_t>(textureDescriptorCount * SceneShaderInputLayout::textureCount)
			});
			poolSizes.emplace_back(VkDescriptorPoolSize{
				VK_DESCRIPTOR_TYPE_SAMPLER,
				static_cast<uint32_t>(textureDescriptorCount * SceneShaderInputLayout::samplerCount)
			});
		}
		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		createInfo.pPoolSizes = poolSizes.data();
		createInfo.maxSets = static_cast<uint32_t>(2 + textureDescriptorCount);
		const VkResult result = vkCreateDescriptorPool(
			device, &createInfo, nullptr, &descriptorPool);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "descriptor pool 생성",
				"Vulkan descriptor pool을 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanDescriptorSet::AllocateDescriptorSets(const VulkanPipeline& sourcePipeline,
		const size_t textureDescriptorCount) {
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = 1;
		const VkDescriptorSetLayout vertexLayout = sourcePipeline.GetVertexDescriptorSetLayout();
		allocateInfo.pSetLayouts = &vertexLayout;
		VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &vertexDescriptorSet);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "vertex descriptor set 할당",
				"Vulkan vertex descriptor set을 할당하지 못했습니다", result, true));
		}
		const VkDescriptorSetLayout pixelLayout = sourcePipeline.GetPixelDescriptorSetLayout();
		allocateInfo.pSetLayouts = &pixelLayout;
		result = vkAllocateDescriptorSets(device, &allocateInfo, &pixelDescriptorSet);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "pixel descriptor set 할당",
				"Vulkan pixel descriptor set을 할당하지 못했습니다", result, true));
		}
		if (textureDescriptorCount == 0)
			return {};
		textureDescriptorSets.resize(textureDescriptorCount);
		const std::vector textureLayouts(
			textureDescriptorCount, sourcePipeline.GetTextureDescriptorSetLayout());
		VkDescriptorSetAllocateInfo textureAllocateInfo{};
		textureAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		textureAllocateInfo.descriptorPool = descriptorPool;
		textureAllocateInfo.descriptorSetCount = static_cast<uint32_t>(textureLayouts.size());
		textureAllocateInfo.pSetLayouts = textureLayouts.data();
		result = vkAllocateDescriptorSets(
			device, &textureAllocateInfo, textureDescriptorSets.data());
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture descriptor set 할당",
				"Vulkan texture descriptor set을 할당하지 못했습니다", result, true));
		}
		return {};
	}

	void VulkanDescriptorSet::UpdateVertexDescriptorSet(const VulkanBuffer& vertexConstantBuffer, const VkDeviceSize vertexConstantRange) const {
		const VkDescriptorBufferInfo vertexBufferInfo{ vertexConstantBuffer.GetBuffer(), 0, vertexConstantRange };
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = vertexDescriptorSet;
		write.dstBinding = SpirvBindingLayout::frameDataBinding;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write.pBufferInfo = &vertexBufferInfo;
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	void VulkanDescriptorSet::UpdatePixelDescriptorSet(const VulkanBuffer& pixelConstantBuffer,
		const VkDeviceSize pixelConstantRange) const {
		const VkDescriptorBufferInfo pixelBufferInfo{ pixelConstantBuffer.GetBuffer(), 0, pixelConstantRange };
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = pixelDescriptorSet;
		write.dstBinding = SpirvBindingLayout::parameterDataBinding;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		write.pBufferInfo = &pixelBufferInfo;
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	GraphicsError::Result<void> VulkanDescriptorSet::UpdateTextureDescriptorSets(std::vector<VulkanModelMaterial>& materials) const {
		if (materials.size() != textureDescriptorSets.size()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "texture descriptor set 갱신",
				"material 수와 Vulkan texture descriptor set 수가 일치하지 않습니다"));
		}
		for (size_t i = 0; i < materials.size(); i++) {
			VulkanModelMaterial& material = materials[i];
			material.textureDescriptorSet = textureDescriptorSets[i];
			const VkDescriptorImageInfo imageInfos[] = {
				{ material.texture.sampler, material.texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
				{ material.toonTexture.sampler, material.toonTexture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
				{ material.sphereTexture.sampler, material.sphereTexture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			};
			const auto MakeImageWrite = [&material](const uint32_t binding, const VkDescriptorType type,
				const VkDescriptorImageInfo& imageInfo) {
				VkWriteDescriptorSet write{};
				write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write.dstSet = material.textureDescriptorSet;
				write.dstBinding = binding;
				write.descriptorCount = 1;
				write.descriptorType = type;
				write.pImageInfo = &imageInfo;
				return write;
			};
			const VkWriteDescriptorSet writes[] = {
				MakeImageWrite(SpirvBindingLayout::ResolveTextureBinding(SceneShaderInputLayout::baseTextureRegister),
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos[0]),
				MakeImageWrite(SpirvBindingLayout::ResolveTextureBinding(SceneShaderInputLayout::toonTextureRegister),
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos[1]),
				MakeImageWrite(SpirvBindingLayout::ResolveTextureBinding(SceneShaderInputLayout::sphereTextureRegister),
					VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos[2]),
				MakeImageWrite(SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::baseSamplerRegister),
					VK_DESCRIPTOR_TYPE_SAMPLER, imageInfos[0]),
				MakeImageWrite(SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::toonSamplerRegister),
					VK_DESCRIPTOR_TYPE_SAMPLER, imageInfos[1]),
				MakeImageWrite(SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::sphereSamplerRegister),
					VK_DESCRIPTOR_TYPE_SAMPLER, imageInfos[2])
			};
			vkUpdateDescriptorSets(device, std::size(writes), writes, 0, nullptr);
		}
		return {};
	}

	VulkanDescriptorSet::~VulkanDescriptorSet() {
		Reset();
	}

	GraphicsError::Result<void> VulkanDescriptorSet::Initialize(const VulkanDevice& sourceDevice,
		const VulkanPipeline& sourcePipeline,
		const VulkanBuffer& vertexConstantBuffer,
		const VkDeviceSize vertexConstantRange,
		const VulkanBuffer& pixelConstantBuffer,
		const VkDeviceSize pixelConstantRange,
		std::vector<VulkanModelMaterial>& materials,
		const bool materialTexturesRequired) {
		Reset();
		device = sourceDevice.GetDevice();
		if (device == VK_NULL_HANDLE) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "descriptor set 초기화",
				"Vulkan device를 사용할 수 없습니다"));
		}
		const size_t textureDescriptorCount = materialTexturesRequired ? materials.size() : 0;
		const auto poolResult = CreateDescriptorPool(textureDescriptorCount);
		if (!poolResult) {
			const GraphicsError error = poolResult.error();
			Reset();
			return std::unexpected(error);
		}
		const auto allocationResult = AllocateDescriptorSets(
			sourcePipeline, textureDescriptorCount);
		if (!allocationResult) {
			const GraphicsError error = allocationResult.error();
			Reset();
			return std::unexpected(error);
		}
		UpdateVertexDescriptorSet(vertexConstantBuffer, vertexConstantRange);
		UpdatePixelDescriptorSet(pixelConstantBuffer, pixelConstantRange);
		if (!materialTexturesRequired)
			return {};
		const auto updateResult = UpdateTextureDescriptorSets(materials);
		if (updateResult)
			return {};
		const GraphicsError error = updateResult.error();
		Reset();
		return std::unexpected(error);
	}

	void VulkanDescriptorSet::Reset() {
		if (device != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		descriptorPool = VK_NULL_HANDLE;
		vertexDescriptorSet = VK_NULL_HANDLE;
		pixelDescriptorSet = VK_NULL_HANDLE;
		textureDescriptorSets.clear();
		device = VK_NULL_HANDLE;
	}
}
