#include "Viewer/Vulkan/Helper/VulkanPostProcess.h"

#include "Viewer/Shader/DxcShaderCompiler.h"
#include "Viewer/Vulkan/Helper/VulkanMemory.h"
#include "Viewer/Vulkan/Helper/VulkanShaderModule.h"

#include <iostream>

namespace Chrivent {
	VulkanPostProcess::~VulkanPostProcess() {
		Reset();
	}

	size_t VulkanPostProcess::ResolveTargetIndex(const size_t targetIndex, const uint32_t imageIndex) const {
		return targetIndex * swapChainImageCount + imageIndex;
	}

	bool VulkanPostProcess::CreateTargetImages(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain) {
		swapChainImageCount = sourceSwapChain.images.size();
		const size_t imageCount = swapChainImageCount * kTargetCount;
		targetImages.resize(imageCount);
		targetImageMemories.resize(imageCount);
		targetImageViews.resize(imageCount);
		for (size_t index = 0; index < targetImages.size(); index++) {
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent = { sourceSwapChain.extent.width, sourceSwapChain.extent.height, 1 };
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = sourceSwapChain.imageFormat;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			if (vkCreateImage(device, &imageInfo, nullptr, &targetImages[index]) != VK_SUCCESS)
				return false;
			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, targetImages[index], &requirements);
			uint32_t memoryType = 0;
			if (!VulkanMemory::FindMemoryType(
				sourceDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType))
				return false;
			VkMemoryAllocateInfo allocateInfo{};
			allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocateInfo.allocationSize = requirements.size;
			allocateInfo.memoryTypeIndex = memoryType;
			if (vkAllocateMemory(device, &allocateInfo, nullptr, &targetImageMemories[index]) != VK_SUCCESS
				|| vkBindImageMemory(device, targetImages[index], targetImageMemories[index], 0) != VK_SUCCESS)
				return false;
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = targetImages[index];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = sourceSwapChain.imageFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(device, &viewInfo, nullptr, &targetImageViews[index]) != VK_SUCCESS)
				return false;
		}
		return true;
	}

	bool VulkanPostProcess::CreateDescriptors(const VulkanSwapChain&) {
		VkDescriptorSetLayoutCreateInfo emptyLayoutInfo{};
		emptyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		if (vkCreateDescriptorSetLayout(device, &emptyLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS
			|| vkCreateDescriptorSetLayout(device, &emptyLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS)
			return false;
		constexpr VkDescriptorSetLayoutBinding bindings[] = {
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding{
				.binding = 3,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			}
		};
		VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
		textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		textureLayoutInfo.bindingCount = 2;
		textureLayoutInfo.pBindings = bindings;
		if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr, &descriptorSetLayouts[2]) != VK_SUCCESS)
			return false;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 3;
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts;
		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
			return false;
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxLod = 1.0f;
		if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
			return false;
		const uint32_t descriptorCount = static_cast<uint32_t>(targetImageViews.size());
		const VkDescriptorPoolSize poolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, descriptorCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, descriptorCount }
		};
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = descriptorCount;
		poolInfo.poolSizeCount = 2;
		poolInfo.pPoolSizes = poolSizes;
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			return false;
		std::vector<VkDescriptorSetLayout> layouts(descriptorCount, descriptorSetLayouts[2]);
		descriptorSets.resize(descriptorCount);
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = descriptorCount;
		allocateInfo.pSetLayouts = layouts.data();
		if (vkAllocateDescriptorSets(device, &allocateInfo, descriptorSets.data()) != VK_SUCCESS)
			return false;
		for (uint32_t index = 0; index < descriptorCount; index++) {
			const VkDescriptorImageInfo imageInfo{
				.sampler = VK_NULL_HANDLE,
				.imageView = targetImageViews[index],
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			const VkDescriptorImageInfo samplerDescriptor{ .sampler = sampler };
			const VkWriteDescriptorSet writes[] = {
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = descriptorSets[index],
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
					.pImageInfo = &imageInfo
				},
				VkWriteDescriptorSet{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = descriptorSets[index],
					.dstBinding = 3,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.pImageInfo = &samplerDescriptor
				}
			};
			vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
		}
		return true;
	}

	bool VulkanPostProcess::CreatePipeline(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain, const std::vector<const EffectDefinition*>& effects) {
		for (const auto* effect : effects) {
			if (!effect || effect->passes.empty())
				continue;
			const auto& pass = effect->passes.front();
		std::vector<uint32_t> vertexCode;
		std::vector<uint32_t> pixelCode;
		std::string error;
		const std::wstring vertexEntry(pass.vertexEntry.begin(), pass.vertexEntry.end());
		const std::wstring pixelEntry(pass.pixelEntry.begin(), pass.pixelEntry.end());
		if (!DxcShaderCompiler::CompileSpirv(
			pass.shaderPath, vertexEntry, L"vs_6_0", SpirvTarget::Vulkan, vertexCode, error, true)
			|| !DxcShaderCompiler::CompileSpirv(
				pass.shaderPath, pixelEntry, L"ps_6_0", SpirvTarget::Vulkan, pixelCode, error)) {
			std::cerr << error << '\n';
			return false;
		}
		VulkanShaderModule vertexShader;
		VulkanShaderModule pixelShader;
		if (!vertexShader.Initialize(sourceDevice, vertexCode) || !pixelShader.Initialize(sourceDevice, pixelCode))
			return false;
		const VkPipelineShaderStageCreateInfo stages[] = {
			VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertexShader.GetShaderModule(),
				.pName = pass.vertexEntry.c_str()
			},
			VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = pixelShader.GetShaderModule(),
				.pName = pass.pixelEntry.c_str()
			}
		};
		VkPipelineVertexInputStateCreateInfo vertexInput{};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		const VkViewport viewport{
			.x = 0.0f, .y = 0.0f,
			.width = static_cast<float>(sourceSwapChain.extent.width),
			.height = static_cast<float>(sourceSwapChain.extent.height),
			.minDepth = 0.0f, .maxDepth = 1.0f
		};
		const VkRect2D scissor{ .extent = sourceSwapChain.extent };
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo blending{};
		blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blending.attachmentCount = 1;
		blending.pAttachments = &blendAttachment;
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &sourceSwapChain.imageFormat
		};
		pipelineInfo.pNext = &renderingInfo;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &blending;
		pipelineInfo.layout = pipelineLayout;
			VkPipeline pipeline = VK_NULL_HANDLE;
			if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
				return false;
			pipelines.push_back(pipeline);
		}
		return true;
	}

	bool VulkanPostProcess::Initialize(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain, const EffectDefinition& effect) {
		std::vector<const EffectDefinition*> effects;
		effects.push_back(&effect);
		return Initialize(sourceDevice, sourceSwapChain, effects);
	}

	bool VulkanPostProcess::Initialize(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain, const std::vector<const EffectDefinition*>& effects) {
		Reset();
		device = sourceDevice.device;
		return CreateTargetImages(sourceDevice, sourceSwapChain)
			&& CreateDescriptors(sourceSwapChain)
			&& CreatePipeline(sourceDevice, sourceSwapChain, effects);
	}

	void VulkanPostProcess::Reset() {
		if (device != VK_NULL_HANDLE) {
			for (const VkPipeline pipeline : pipelines) {
				if (pipeline != VK_NULL_HANDLE)
					vkDestroyPipeline(device, pipeline, nullptr);
			}
			if (pipelineLayout != VK_NULL_HANDLE)
				vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			if (descriptorPool != VK_NULL_HANDLE)
				vkDestroyDescriptorPool(device, descriptorPool, nullptr);
			for (const VkDescriptorSetLayout layout : descriptorSetLayouts) {
				if (layout != VK_NULL_HANDLE)
					vkDestroyDescriptorSetLayout(device, layout, nullptr);
			}
			if (sampler != VK_NULL_HANDLE)
				vkDestroySampler(device, sampler, nullptr);
			for (const VkImageView view : targetImageViews)
				vkDestroyImageView(device, view, nullptr);
			for (const VkImage image : targetImages)
				vkDestroyImage(device, image, nullptr);
			for (const VkDeviceMemory memory : targetImageMemories)
				vkFreeMemory(device, memory, nullptr);
		}
		descriptorSets.clear();
		targetImageViews.clear();
		targetImages.clear();
		targetImageMemories.clear();
		for (VkDescriptorSetLayout& layout : descriptorSetLayouts)
			layout = VK_NULL_HANDLE;
		descriptorPool = VK_NULL_HANDLE;
		pipelineLayout = VK_NULL_HANDLE;
		pipelines.clear();
		sampler = VK_NULL_HANDLE;
		swapChainImageCount = 0;
		device = VK_NULL_HANDLE;
	}
}
