#include "Viewer/Vulkan/VulkanPostProcess.h"

#include "Viewer/Shader/DxcShaderCompiler.h"
#include "Viewer/Shader/PostProcessInputLayout.h"
#include "Viewer/Viewer.h"
#include "Viewer/Vulkan/Helper/VulkanMemory.h"
#include "Viewer/Vulkan/Helper/VulkanShaderModule.h"

#include <iostream>

namespace Chrivent {
	VulkanPostProcess::~VulkanPostProcess() {
		VulkanPostProcess::Reset();
	}

	size_t VulkanPostProcess::ResolveTargetIndex(const size_t targetIndex, const uint32_t imageIndex) const {
		return targetIndex * swapChainImageCount + imageIndex;
	}

	size_t VulkanPostProcess::ResolveFocusHistoryIndex(const size_t historyIndex, const uint32_t imageIndex) const {
		return historyIndex * swapChainImageCount + imageIndex;
	}

	size_t VulkanPostProcess::ResolveDescriptorIndex(
		const size_t targetIndex, const uint32_t imageIndex, const size_t historyIndex) const {
		return historyIndex * targetImages.size() + ResolveTargetIndex(targetIndex, imageIndex);
	}

	bool VulkanPostProcess::CreateTargetImages(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain) {
		swapChainImageCount = sourceSwapChain.images.size();
		const size_t imageCount = swapChainImageCount * targetCount;
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

	bool VulkanPostProcess::CreateDepthImages(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain, const VkFormat depthFormat) {
		depthImages.resize(swapChainImageCount);
		depthImageMemories.resize(swapChainImageCount);
		depthImageViews.resize(swapChainImageCount);
		for (size_t index = 0; index < swapChainImageCount; index++) {
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent = { sourceSwapChain.extent.width, sourceSwapChain.extent.height, 1 };
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = depthFormat;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			if (vkCreateImage(device, &imageInfo, nullptr, &depthImages[index]) != VK_SUCCESS)
				return false;
			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, depthImages[index], &requirements);
			uint32_t memoryType = 0;
			if (!VulkanMemory::FindMemoryType(
				sourceDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType))
				return false;
			VkMemoryAllocateInfo allocateInfo{};
			allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocateInfo.allocationSize = requirements.size;
			allocateInfo.memoryTypeIndex = memoryType;
			if (vkAllocateMemory(device, &allocateInfo, nullptr, &depthImageMemories[index]) != VK_SUCCESS
				|| vkBindImageMemory(device, depthImages[index], depthImageMemories[index], 0) != VK_SUCCESS)
				return false;
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = depthImages[index];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = depthFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageViews[index]) != VK_SUCCESS)
				return false;
		}
		return true;
	}

	bool VulkanPostProcess::CreateFocusHistoryImages(const VulkanDevice& sourceDevice) {
		const size_t imageCount = swapChainImageCount * historyTargetCount;
		focusHistoryImages.resize(imageCount);
		focusHistoryImageMemories.resize(imageCount);
		focusHistoryImageViews.resize(imageCount);
		for (size_t index = 0; index < focusHistoryImages.size(); index++) {
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent = { 1, 1, 1 };
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			if (vkCreateImage(device, &imageInfo, nullptr, &focusHistoryImages[index]) != VK_SUCCESS)
				return false;
			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, focusHistoryImages[index], &requirements);
			uint32_t memoryType = 0;
			if (!VulkanMemory::FindMemoryType(
				sourceDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType))
				return false;
			VkMemoryAllocateInfo allocateInfo{};
			allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocateInfo.allocationSize = requirements.size;
			allocateInfo.memoryTypeIndex = memoryType;
			if (vkAllocateMemory(device, &allocateInfo, nullptr, &focusHistoryImageMemories[index]) != VK_SUCCESS
				|| vkBindImageMemory(device, focusHistoryImages[index], focusHistoryImageMemories[index], 0) != VK_SUCCESS)
				return false;
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = focusHistoryImages[index];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.layerCount = 1;
			if (vkCreateImageView(device, &viewInfo, nullptr, &focusHistoryImageViews[index]) != VK_SUCCESS)
				return false;
		}
		focusHistoryIndices.assign(swapChainImageCount, 0);
		focusHistoryInitialized.assign(swapChainImageCount, false);
		return true;
	}

	bool VulkanPostProcess::CreateFrameDataBuffers(const VulkanDevice& sourceDevice) {
		frameDataBuffers.clear();
		frameDataBuffers.reserve(swapChainImageCount);
		for (size_t index = 0; index < swapChainImageCount; index++) {
			auto buffer = std::make_unique<VulkanBuffer>();
			if (!buffer->Initialize(sourceDevice, sizeof(PostProcessFrameData),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				return false;
			frameDataBuffers.push_back(std::move(buffer));
		}
		return true;
	}

	bool VulkanPostProcess::CreateDescriptors(const VulkanSwapChain&) {
		constexpr VkDescriptorSetLayoutBinding frameDataBinding{
			.binding = PostProcessInputLayout::frameDataRegister,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		VkDescriptorSetLayoutCreateInfo frameDataLayoutInfo{};
		frameDataLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		frameDataLayoutInfo.bindingCount = 1;
		frameDataLayoutInfo.pBindings = &frameDataBinding;
		VkDescriptorSetLayoutCreateInfo emptyLayoutInfo{};
		emptyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		if (vkCreateDescriptorSetLayout(
			device, &frameDataLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS
			|| vkCreateDescriptorSetLayout(
				device, &emptyLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS)
			return false;
		constexpr VkDescriptorSetLayoutBinding bindings[] = {
			VkDescriptorSetLayoutBinding{
				.binding = PostProcessInputLayout::sceneColorRegister,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding{
				.binding = PostProcessInputLayout::sceneDepthRegister,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding{
				.binding = PostProcessInputLayout::focusHistoryRegister,
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
		textureLayoutInfo.bindingCount = 4;
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
		const uint32_t mainDescriptorCount = static_cast<uint32_t>(targetImageViews.size() * historyTargetCount);
		const uint32_t focusDescriptorCount = static_cast<uint32_t>(swapChainImageCount * historyTargetCount);
		const uint32_t textureDescriptorCount = mainDescriptorCount + focusDescriptorCount;
		const uint32_t frameDescriptorCount = static_cast<uint32_t>(swapChainImageCount);
		const uint32_t descriptorCount = frameDescriptorCount + textureDescriptorCount;
		const VkDescriptorPoolSize poolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameDescriptorCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureDescriptorCount * 3 },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, textureDescriptorCount }
		};
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = descriptorCount;
		poolInfo.poolSizeCount = 3;
		poolInfo.pPoolSizes = poolSizes;
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			return false;
		std::vector<VkDescriptorSetLayout> layouts(descriptorCount, descriptorSetLayouts[2]);
		for (uint32_t index = 0; index < frameDescriptorCount; index++)
			layouts[index] = descriptorSetLayouts[0];
		std::vector<VkDescriptorSet> allocatedSets(descriptorCount);
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = descriptorCount;
		allocateInfo.pSetLayouts = layouts.data();
		if (vkAllocateDescriptorSets(device, &allocateInfo, allocatedSets.data()) != VK_SUCCESS)
			return false;
		frameDataDescriptorSets.assign(allocatedSets.begin(), allocatedSets.begin() + frameDescriptorCount);
		descriptorSets.assign(allocatedSets.begin() + frameDescriptorCount,
			allocatedSets.begin() + frameDescriptorCount + mainDescriptorCount);
		focusHistoryDescriptorSets.assign(
			allocatedSets.begin() + frameDescriptorCount + mainDescriptorCount, allocatedSets.end());
		for (uint32_t imageIndex = 0; imageIndex < frameDescriptorCount; imageIndex++) {
			const VkDescriptorBufferInfo bufferInfo{
				.buffer = frameDataBuffers[imageIndex]->buffer,
				.offset = 0,
				.range = sizeof(PostProcessFrameData)
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frameDataDescriptorSets[imageIndex],
				.dstBinding = PostProcessInputLayout::frameDataRegister,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			};
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
		for (size_t historyIndex = 0; historyIndex < historyTargetCount; historyIndex++) {
			for (uint32_t targetIndex = 0; targetIndex < targetCount; targetIndex++) {
				for (uint32_t imageIndex = 0; imageIndex < swapChainImageCount; imageIndex++) {
					const size_t targetFlatIndex = ResolveTargetIndex(targetIndex, imageIndex);
					const size_t descriptorIndex = ResolveDescriptorIndex(targetIndex, imageIndex, historyIndex);
					const VkDescriptorImageInfo imageInfo{
						.sampler = VK_NULL_HANDLE,
						.imageView = targetImageViews[targetFlatIndex],
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
					const VkDescriptorImageInfo depthInfo{
						.sampler = VK_NULL_HANDLE,
						.imageView = depthImageViews[imageIndex],
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
					const VkDescriptorImageInfo focusHistoryInfo{
						.sampler = VK_NULL_HANDLE,
						.imageView = focusHistoryImageViews[ResolveFocusHistoryIndex(historyIndex, imageIndex)],
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
					const VkDescriptorImageInfo samplerDescriptor{ .sampler = sampler };
					const VkWriteDescriptorSet writes[] = {
						VkWriteDescriptorSet{
							.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
							.dstSet = descriptorSets[descriptorIndex],
							.dstBinding = PostProcessInputLayout::sceneColorRegister,
							.descriptorCount = 1,
							.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
							.pImageInfo = &imageInfo
						},
						VkWriteDescriptorSet{
							.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
							.dstSet = descriptorSets[descriptorIndex],
							.dstBinding = PostProcessInputLayout::sceneDepthRegister,
							.descriptorCount = 1,
							.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
							.pImageInfo = &depthInfo
						},
						VkWriteDescriptorSet{
							.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
							.dstSet = descriptorSets[descriptorIndex],
							.dstBinding = PostProcessInputLayout::focusHistoryRegister,
							.descriptorCount = 1,
							.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
							.pImageInfo = &focusHistoryInfo
						},
						VkWriteDescriptorSet{
							.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
							.dstSet = descriptorSets[descriptorIndex],
							.dstBinding = 3,
							.descriptorCount = 1,
							.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
							.pImageInfo = &samplerDescriptor
						}
					};
					vkUpdateDescriptorSets(device, 4, writes, 0, nullptr);
				}
			}
			for (uint32_t imageIndex = 0; imageIndex < swapChainImageCount; imageIndex++) {
				const VkDescriptorImageInfo imageInfo{
					.sampler = VK_NULL_HANDLE,
					.imageView = targetImageViews[ResolveTargetIndex(0, imageIndex)],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};
				const VkDescriptorImageInfo depthInfo{
					.sampler = VK_NULL_HANDLE,
					.imageView = depthImageViews[imageIndex],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};
				const VkDescriptorImageInfo focusHistoryInfo{
					.sampler = VK_NULL_HANDLE,
					.imageView = focusHistoryImageViews[ResolveFocusHistoryIndex(historyIndex, imageIndex)],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};
				const VkDescriptorImageInfo samplerDescriptor{ .sampler = sampler };
				const VkWriteDescriptorSet writes[] = {
					VkWriteDescriptorSet{
						.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.dstSet = focusHistoryDescriptorSets[ResolveFocusHistoryIndex(historyIndex, imageIndex)],
						.dstBinding = PostProcessInputLayout::sceneColorRegister,
						.descriptorCount = 1,
						.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
						.pImageInfo = &imageInfo
					},
					VkWriteDescriptorSet{
						.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.dstSet = focusHistoryDescriptorSets[ResolveFocusHistoryIndex(historyIndex, imageIndex)],
						.dstBinding = PostProcessInputLayout::sceneDepthRegister,
						.descriptorCount = 1,
						.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
						.pImageInfo = &depthInfo
					},
					VkWriteDescriptorSet{
						.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.dstSet = focusHistoryDescriptorSets[ResolveFocusHistoryIndex(historyIndex, imageIndex)],
						.dstBinding = PostProcessInputLayout::focusHistoryRegister,
						.descriptorCount = 1,
						.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
						.pImageInfo = &focusHistoryInfo
					},
					VkWriteDescriptorSet{
						.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.dstSet = focusHistoryDescriptorSets[ResolveFocusHistoryIndex(historyIndex, imageIndex)],
						.dstBinding = 3,
						.descriptorCount = 1,
						.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
						.pImageInfo = &samplerDescriptor
					}
				};
				vkUpdateDescriptorSets(device, 4, writes, 0, nullptr);
			}
		}
		return true;
	}

	bool VulkanPostProcess::CreateGraphicsPipeline(const VulkanDevice& sourceDevice,
		const EffectPassDefinition& pass, const VkExtent2D extent, const VkFormat format,
		VkPipeline& pipeline) const {
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
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = vertexShader.GetShaderModule(),
				.pName = pass.vertexEntry.c_str()
			},
			{
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
			.width = static_cast<float>(extent.width), .height = static_cast<float>(extent.height),
			.minDepth = 0.0f, .maxDepth = 1.0f
		};
		const VkRect2D scissor{ .extent = extent };
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
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &format
		};
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
		return vkCreateGraphicsPipelines(
			device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) == VK_SUCCESS;
	}

	bool VulkanPostProcess::CreatePipelines(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		for (const auto& pass : ResolvePasses()) {
			VkPipeline pipeline = VK_NULL_HANDLE;
			if (!CreateGraphicsPipeline(
				sourceDevice, pass, sourceSwapChain.extent, sourceSwapChain.imageFormat, pipeline))
				return false;
			pipelines.push_back(pipeline);
		}
		const auto* historyPass = ResolveHistoryPass();
		return !historyPass || CreateGraphicsPipeline(sourceDevice, *historyPass,
			VkExtent2D{ 1, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT, focusHistoryPipeline);
	}

	void VulkanPostProcess::AdvanceFocusHistory(const uint32_t imageIndex) {
		if (imageIndex >= focusHistoryIndices.size())
			return;
		focusHistoryIndices[imageIndex] = ResolveNextHistoryIndex(focusHistoryIndices[imageIndex]);
		focusHistoryInitialized[imageIndex] = true;
	}

	bool VulkanPostProcess::Initialize(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain, const VkFormat depthFormat) {
		Reset();
		device = sourceDevice.device;
		return CreateTargetImages(sourceDevice, sourceSwapChain)
			&& CreateDepthImages(sourceDevice, sourceSwapChain, depthFormat)
			&& CreateFocusHistoryImages(sourceDevice)
			&& CreateFrameDataBuffers(sourceDevice)
			&& CreateDescriptors(sourceSwapChain)
			&& CreatePipelines(sourceDevice, sourceSwapChain);
	}

	void VulkanPostProcess::ResetHistory() {
		for (size_t& index : focusHistoryIndices)
			index = 0;
		for (size_t index = 0; index < focusHistoryInitialized.size(); index++)
			focusHistoryInitialized[index] = false;
	}

	void VulkanPostProcess::Reset() {
		if (device != VK_NULL_HANDLE) {
			for (const VkPipeline pipeline : pipelines) {
				if (pipeline != VK_NULL_HANDLE)
					vkDestroyPipeline(device, pipeline, nullptr);
			}
			if (focusHistoryPipeline != VK_NULL_HANDLE)
				vkDestroyPipeline(device, focusHistoryPipeline, nullptr);
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
			for (const VkImageView view : depthImageViews)
				vkDestroyImageView(device, view, nullptr);
			for (const VkImageView view : focusHistoryImageViews)
				vkDestroyImageView(device, view, nullptr);
			for (const VkImage image : targetImages)
				vkDestroyImage(device, image, nullptr);
			for (const VkImage image : depthImages)
				vkDestroyImage(device, image, nullptr);
			for (const VkImage image : focusHistoryImages)
				vkDestroyImage(device, image, nullptr);
			for (const VkDeviceMemory memory : targetImageMemories)
				vkFreeMemory(device, memory, nullptr);
			for (const VkDeviceMemory memory : depthImageMemories)
				vkFreeMemory(device, memory, nullptr);
			for (const VkDeviceMemory memory : focusHistoryImageMemories)
				vkFreeMemory(device, memory, nullptr);
		}
		descriptorSets.clear();
		focusHistoryDescriptorSets.clear();
		frameDataDescriptorSets.clear();
		frameDataBuffers.clear();
		targetImageViews.clear();
		targetImages.clear();
		targetImageMemories.clear();
		depthImageViews.clear();
		depthImages.clear();
		depthImageMemories.clear();
		focusHistoryImageViews.clear();
		focusHistoryImages.clear();
		focusHistoryImageMemories.clear();
		focusHistoryIndices.clear();
		focusHistoryInitialized.clear();
		for (VkDescriptorSetLayout& layout : descriptorSetLayouts)
			layout = VK_NULL_HANDLE;
		descriptorPool = VK_NULL_HANDLE;
		pipelineLayout = VK_NULL_HANDLE;
		focusHistoryPipeline = VK_NULL_HANDLE;
		pipelines.clear();
		sampler = VK_NULL_HANDLE;
		swapChainImageCount = 0;
		device = VK_NULL_HANDLE;
	}
}
