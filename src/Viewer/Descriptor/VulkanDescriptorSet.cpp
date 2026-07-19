#include "Viewer/Descriptor/VulkanDescriptorSet.h"

#include "Viewer/DrawResource/VulkanModelResources.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

namespace Chrivent {
	GraphicsResult<void> VulkanDescriptorSet::CreateDescriptorPool(const size_t textureDescriptorCount) {
		constexpr size_t maximumDescriptorCount = std::numeric_limits<uint32_t>::max();
		if (textureDescriptorCount > maximumDescriptorCount / 3) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "descriptor pool 생성",
				"texture descriptor 개수가 Vulkan 범위를 벗어났습니다"));
		}
		std::vector poolSizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				.descriptorCount = 2
			}
		};
		if (textureDescriptorCount > 0) {
			poolSizes.emplace_back(VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = static_cast<uint32_t>(textureDescriptorCount * 3)
			});
			poolSizes.emplace_back(VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = static_cast<uint32_t>(textureDescriptorCount * 3)
			});
		}
		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = poolSizes.size();
		createInfo.pPoolSizes = poolSizes.data();
		createInfo.maxSets = static_cast<uint32_t>(2 + textureDescriptorCount);
		const VkResult result = vkCreateDescriptorPool(
			device, &createInfo, nullptr, &descriptorPool);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "descriptor pool 생성",
				"Vulkan descriptor pool을 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> VulkanDescriptorSet::AllocateDescriptorSets(const VulkanPipeline& sourcePipeline,
		const size_t textureDescriptorCount) {
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = 1;
		const VkDescriptorSetLayout vertexLayout = sourcePipeline.GetVertexDescriptorSetLayout();
		allocateInfo.pSetLayouts = &vertexLayout;
		VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &vertexDescriptorSet);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "vertex descriptor set 할당",
				"Vulkan vertex descriptor set을 할당하지 못했습니다", result, true));
		}
		const VkDescriptorSetLayout pixelLayout = sourcePipeline.GetPixelDescriptorSetLayout();
		allocateInfo.pSetLayouts = &pixelLayout;
		result = vkAllocateDescriptorSets(device, &allocateInfo, &pixelDescriptorSet);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
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
		textureAllocateInfo.descriptorSetCount = textureLayouts.size();
		textureAllocateInfo.pSetLayouts = textureLayouts.data();
		result = vkAllocateDescriptorSets(
			device, &textureAllocateInfo, textureDescriptorSets.data());
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture descriptor set 할당",
				"Vulkan texture descriptor set을 할당하지 못했습니다", result, true));
		}
		return {};
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
			.dstBinding = SpirvBindingLayout::frameDataBinding,
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
			.dstBinding = SpirvBindingLayout::parameterDataBinding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.pImageInfo = nullptr,
			.pBufferInfo = &pixelBufferInfo,
			.pTexelBufferView = nullptr
		};
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
	}

	GraphicsResult<void> VulkanDescriptorSet::UpdateTextureDescriptorSets(std::vector<VulkanModelMaterial>& materials) const {
		if (materials.size() != textureDescriptorSets.size()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "texture descriptor set 갱신",
				"material 수와 Vulkan texture descriptor set 수가 일치하지 않습니다"));
		}
		for (size_t i = 0; i < materials.size(); i++) {
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
					.dstBinding = SpirvBindingLayout::ResolveTextureBinding(
						SceneShaderInputLayout::baseTextureRegister),
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
					.dstBinding = SpirvBindingLayout::ResolveTextureBinding(
						SceneShaderInputLayout::toonTextureRegister),
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
					.dstBinding = SpirvBindingLayout::ResolveTextureBinding(
						SceneShaderInputLayout::sphereTextureRegister),
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
					.dstBinding = SpirvBindingLayout::ResolveSamplerBinding(
						SceneShaderInputLayout::baseSamplerRegister),
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &imageInfos[0]
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = material.textureDescriptorSet,
					.dstBinding = SpirvBindingLayout::ResolveSamplerBinding(
						SceneShaderInputLayout::toonSamplerRegister),
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &imageInfos[1]
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = material.textureDescriptorSet,
					.dstBinding = SpirvBindingLayout::ResolveSamplerBinding(
						SceneShaderInputLayout::sphereSamplerRegister),
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &imageInfos[2]
				}
			};
			vkUpdateDescriptorSets(device, std::size(writes), writes, 0, nullptr);
		}
		return {};
	}

	VulkanDescriptorSet::~VulkanDescriptorSet() {
		Reset();
	}

	GraphicsResult<void> VulkanDescriptorSet::Initialize(const VulkanDevice& sourceDevice,
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
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
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
