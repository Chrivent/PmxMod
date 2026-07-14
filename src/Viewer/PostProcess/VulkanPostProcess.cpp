#include "Viewer/PostProcess/VulkanPostProcess.h"

#include "Viewer/Shader/DxcShaderCompiler.h"
#include "Viewer/Effect/PostProcessInputLayout.h"
#include "Viewer/Viewer/Viewer.h"
#include "Viewer/Command/VulkanCommandBuffer.h"
#include "Viewer/Memory/VulkanMemory.h"
#include "Viewer/Shader/VulkanShaderModule.h"

#include <iostream>

namespace Chrivent {
	VulkanPostProcess::~VulkanPostProcess() {
		VulkanPostProcess::Reset();
	}

	bool VulkanPostProcess::CreateColorImage(const VulkanDevice& sourceDevice, const VkExtent2D extent,
		const VkFormat format, const VkImageUsageFlags usage, VkImage& image,
		VkDeviceMemory& memory, VkImageView& imageView) const {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = { extent.width, extent.height, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
			return false;
		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(device, image, &requirements);
		uint32_t memoryType = 0;
		if (!VulkanMemory::FindMemoryType(
			sourceDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType))
			return false;
		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = requirements.size;
		allocateInfo.memoryTypeIndex = memoryType;
		if (vkAllocateMemory(device, &allocateInfo, nullptr, &memory) != VK_SUCCESS
			|| vkBindImageMemory(device, image, memory, 0) != VK_SUCCESS)
			return false;
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		return vkCreateImageView(device, &viewInfo, nullptr, &imageView) == VK_SUCCESS;
	}

	bool VulkanPostProcess::CreateSceneImages(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		swapChainImageCount = sourceSwapChain.images.size();
		targetExtent = sourceSwapChain.extent;
		swapChainFormat = sourceSwapChain.imageFormat;
		sceneImages.resize(swapChainImageCount);
		sceneImageMemories.resize(swapChainImageCount);
		sceneImageViews.resize(swapChainImageCount);
		for (size_t index = 0; index < swapChainImageCount; index++) {
			if (!CreateColorImage(sourceDevice, targetExtent, swapChainFormat,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				sceneImages[index], sceneImageMemories[index], sceneImageViews[index]))
				return false;
		}
		return true;
	}

	bool VulkanPostProcess::CreateDepthImages(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain, const VkFormat depthFormat) {
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

	bool VulkanPostProcess::CreateVelocityImages(const VulkanDevice& sourceDevice) {
		velocityImages.resize(swapChainImageCount);
		velocityImageMemories.resize(swapChainImageCount);
		velocityImageViews.resize(swapChainImageCount);
		velocityImageInitialized.assign(swapChainImageCount, false);
		for (size_t index = 0; index < swapChainImageCount; index++) {
			if (!CreateColorImage(sourceDevice, targetExtent, VK_FORMAT_R16G16_SFLOAT,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				velocityImages[index], velocityImageMemories[index], velocityImageViews[index]))
				return false;
		}
		return true;
	}

	VkFormat VulkanPostProcess::ResolveResourceFormat(const PostProcessResourcePlan& resource) {
		if (resource.format == EffectTextureFormat::Rgba8Unorm)
			return VK_FORMAT_R8G8B8A8_UNORM;
		return resource.format == EffectTextureFormat::Rgba16Float
			? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
	}

	VkExtent2D VulkanPostProcess::ResolveResourceExtent(const PostProcessResourcePlan& resource) const {
		return {
			static_cast<uint32_t>(PostProcess::ResolveResourceExtent(
				static_cast<int>(targetExtent.width), resource, true)),
			static_cast<uint32_t>(PostProcess::ResolveResourceExtent(
				static_cast<int>(targetExtent.height), resource, false))
		};
	}

	bool VulkanPostProcess::CreateEffectResources(const VulkanDevice& sourceDevice) {
		const auto& plans = ResolveResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const PostProcessResourcePlan& plan = plans[resourceIndex];
			VulkanPostProcessResource& resource = resources[resourceIndex];
			const size_t imageCount = plan.lifetime == EffectResourceLifetime::History ? 2 : swapChainImageCount;
			resource.images.resize(imageCount);
			resource.memories.resize(imageCount);
			resource.imageViews.resize(imageCount);
			resource.transientInitialized.assign(
				plan.lifetime == EffectResourceLifetime::Transient ? imageCount : 0, false);
			const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
				| (plan.lifetime == EffectResourceLifetime::History ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);
			for (size_t index = 0; index < imageCount; index++) {
				if (!CreateColorImage(sourceDevice, ResolveResourceExtent(plan), ResolveResourceFormat(plan), usage,
					resource.images[index], resource.memories[index], resource.imageViews[index]))
					return false;
			}
		}
		ResetHistory();
		return true;
	}

	bool VulkanPostProcess::CreateFrameDataBuffers(const VulkanDevice& sourceDevice) {
		frameDataBuffers.clear();
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

	bool VulkanPostProcess::CreateDescriptors() {
		constexpr VkDescriptorSetLayoutBinding frameDataBinding{
			.binding = PostProcessInputLayout::frameDataRegister,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		VkDescriptorSetLayoutCreateInfo frameLayoutInfo{};
		frameLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		frameLayoutInfo.bindingCount = 1;
		frameLayoutInfo.pBindings = &frameDataBinding;
		VkDescriptorSetLayoutCreateInfo emptyLayoutInfo{};
		emptyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		if (vkCreateDescriptorSetLayout(device, &frameLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS
			|| vkCreateDescriptorSetLayout(device, &emptyLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS)
			return false;
		std::vector<VkDescriptorSetLayoutBinding> textureBindings;
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			textureBindings.emplace_back(VkDescriptorSetLayoutBinding{
				.binding = PostProcessInputLayout::ResolveSpirvTextureBinding(slot),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			});
		}
		for (uint32_t samplerSlot = 0; samplerSlot < 3; samplerSlot++) {
			textureBindings.emplace_back(VkDescriptorSetLayoutBinding{
				.binding = 4 + samplerSlot,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			});
		}
		VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
		textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		textureLayoutInfo.bindingCount = static_cast<uint32_t>(textureBindings.size());
		textureLayoutInfo.pBindings = textureBindings.data();
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
		const uint32_t frameSetCount = static_cast<uint32_t>(swapChainImageCount);
		const uint32_t textureSetCount = static_cast<uint32_t>(swapChainImageCount * ResolvePassRoutes().size());
		const VkDescriptorPoolSize poolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameSetCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureSetCount * PostProcessInputLayout::maxTextureCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, textureSetCount * 3 }
		};
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = frameSetCount + textureSetCount;
		poolInfo.poolSizeCount = 3;
		poolInfo.pPoolSizes = poolSizes;
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			return false;
		std::vector layouts(frameSetCount + textureSetCount, descriptorSetLayouts[2]);
		for (uint32_t index = 0; index < frameSetCount; index++)
			layouts[index] = descriptorSetLayouts[0];
		std::vector<VkDescriptorSet> sets(layouts.size());
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
		allocateInfo.pSetLayouts = layouts.data();
		if (vkAllocateDescriptorSets(device, &allocateInfo, sets.data()) != VK_SUCCESS)
			return false;
		frameDataDescriptorSets.assign(sets.begin(), sets.begin() + frameSetCount);
		textureDescriptorSets.assign(sets.begin() + frameSetCount, sets.end());
		for (uint32_t index = 0; index < frameSetCount; index++) {
			const VkDescriptorBufferInfo bufferInfo{
				.buffer = frameDataBuffers[index]->buffer,
				.offset = 0,
				.range = sizeof(PostProcessFrameData)
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frameDataDescriptorSets[index],
				.dstBinding = PostProcessInputLayout::frameDataRegister,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			};
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
		return true;
	}

	size_t VulkanPostProcess::ResolveTextureDescriptorIndex(
		const uint32_t imageIndex, const size_t passIndex) const {
		return imageIndex * ResolvePassRoutes().size() + passIndex;
	}

	VkImageView VulkanPostProcess::ResolveInputImageView(
		const PostProcessPassInputRoute& input, const uint32_t imageIndex) const {
		if (input.kind == PostProcessInputKind::SceneColor)
			return imageIndex < sceneImageViews.size() ? sceneImageViews[imageIndex] : VK_NULL_HANDLE;
		if (input.kind == PostProcessInputKind::SceneDepth)
			return imageIndex < depthImageViews.size() ? depthImageViews[imageIndex] : VK_NULL_HANDLE;
		if (input.kind == PostProcessInputKind::SceneVelocity)
			return imageIndex < velocityImageViews.size() ? velocityImageViews[imageIndex] : VK_NULL_HANDLE;
		if (input.resourceIndex >= resources.size())
			return VK_NULL_HANDLE;
		const VulkanPostProcessResource& resource = resources[input.resourceIndex];
		const size_t index = ResolveResourcePlans()[input.resourceIndex].lifetime
			== EffectResourceLifetime::History ? resource.historyIndex : imageIndex;
		return index < resource.imageViews.size() ? resource.imageViews[index] : VK_NULL_HANDLE;
	}

	void VulkanPostProcess::UpdateTextureDescriptorSet(
		const uint32_t imageIndex, const size_t passIndex) const {
		const size_t descriptorIndex = ResolveTextureDescriptorIndex(imageIndex, passIndex);
		if (descriptorIndex >= textureDescriptorSets.size() || passIndex >= ResolvePassRoutes().size())
			return;
		std::vector<const PostProcessPassInputRoute*> slots(PostProcessInputLayout::maxTextureCount);
		for (const auto& input : ResolvePassRoutes()[passIndex].inputs)
			slots[input.slot] = &input;
		std::vector<VkDescriptorImageInfo> imageInfos(PostProcessInputLayout::maxTextureCount + 3);
		std::vector<VkWriteDescriptorSet> writes;
		writes.reserve(imageInfos.size());
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			VkImageView imageView = imageIndex < sceneImageViews.size() ? sceneImageViews[imageIndex] : VK_NULL_HANDLE;
			if (slots[slot] != nullptr)
				imageView = ResolveInputImageView(*slots[slot], imageIndex);
			imageInfos[slot] = {
				.sampler = VK_NULL_HANDLE,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			writes.emplace_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = textureDescriptorSets[descriptorIndex],
				.dstBinding = PostProcessInputLayout::ResolveSpirvTextureBinding(slot),
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.pImageInfo = &imageInfos[slot]
			});
		}
		for (uint32_t samplerSlot = 0; samplerSlot < 3; samplerSlot++) {
			const size_t infoIndex = PostProcessInputLayout::maxTextureCount + samplerSlot;
			imageInfos[infoIndex] = { .sampler = sampler };
			writes.emplace_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = textureDescriptorSets[descriptorIndex],
				.dstBinding = 4 + samplerSlot,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.pImageInfo = &imageInfos[infoIndex]
			});
		}
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
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
			{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vertexShader.GetShaderModule(),
				.pName = pass.vertexEntry.c_str() },
			{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = pixelShader.GetShaderModule(),
				.pName = pass.pixelEntry.c_str() }
		};
		VkPipelineVertexInputStateCreateInfo vertexInput{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};
		const VkViewport viewport{ .width = static_cast<float>(extent.width),
			.height = static_cast<float>(extent.height), .minDepth = 0.0f, .maxDepth = 1.0f };
		const VkRect2D scissor{ .extent = extent };
		VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor
		};
		VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f
		};
		VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		};
		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo blending{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1, .pAttachments = &blendAttachment
		};
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1, .pColorAttachmentFormats = &format
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

	bool VulkanPostProcess::CreatePipelines(const VulkanDevice& sourceDevice) {
		const auto& passes = ResolvePasses();
		const auto& routes = ResolvePassRoutes();
		for (size_t index = 0; index < passes.size(); index++) {
			VkExtent2D extent = targetExtent;
			VkFormat format = swapChainFormat;
			if (routes[index].outputKind == PostProcessOutputKind::Resource) {
				const PostProcessResourcePlan& resource = ResolveResourcePlans()[routes[index].outputResourceIndex];
				extent = ResolveResourceExtent(resource);
				format = ResolveResourceFormat(resource);
			}
			VkPipeline pipeline = VK_NULL_HANDLE;
			if (!CreateGraphicsPipeline(sourceDevice, passes[index], extent, format, pipeline))
				return false;
			pipelines.emplace_back(pipeline);
		}
		return true;
	}

	bool VulkanPostProcess::ResolveOutputImage(const PostProcessPassRoute& route, const uint32_t imageIndex,
		const VkImage swapChainImage, const VkImageView swapChainImageView, VkImage& image,
		VkImageView& imageView, VkExtent2D& extent, bool& initialized) {
		if (route.outputKind == PostProcessOutputKind::Present) {
			image = swapChainImage;
			imageView = swapChainImageView;
			extent = targetExtent;
			initialized = false;
			return image != VK_NULL_HANDLE && imageView != VK_NULL_HANDLE;
		}
		if (route.outputResourceIndex >= resources.size())
			return false;
		VulkanPostProcessResource& resource = resources[route.outputResourceIndex];
		const PostProcessResourcePlan& plan = ResolveResourcePlans()[route.outputResourceIndex];
		const size_t index = plan.lifetime == EffectResourceLifetime::History
			? ResolveNextHistoryIndex(resource.historyIndex) : imageIndex;
		if (index >= resource.images.size())
			return false;
		image = resource.images[index];
		imageView = resource.imageViews[index];
		extent = ResolveResourceExtent(plan);
		initialized = plan.lifetime == EffectResourceLifetime::History
			? resource.historyInitialized : resource.transientInitialized[index];
		if (plan.lifetime == EffectResourceLifetime::Transient)
			resource.transientInitialized[index] = true;
		return true;
	}

	void VulkanPostProcess::AdvanceHistory(const PostProcessPassRoute& route) {
		if (route.outputKind != PostProcessOutputKind::Resource
			|| route.outputResourceIndex >= resources.size()
			|| ResolveResourcePlans()[route.outputResourceIndex].lifetime != EffectResourceLifetime::History)
			return;
		VulkanPostProcessResource& resource = resources[route.outputResourceIndex];
		resource.historyIndex = ResolveNextHistoryIndex(resource.historyIndex);
		resource.historyInitialized = true;
	}

	void VulkanPostProcess::DestroyImages(std::vector<VkImage>& images,
		std::vector<VkDeviceMemory>& memories, std::vector<VkImageView>& imageViews) const {
		for (const VkImageView view : imageViews) {
			if (view != VK_NULL_HANDLE)
				vkDestroyImageView(device, view, nullptr);
		}
		for (const VkImage image : images) {
			if (image != VK_NULL_HANDLE)
				vkDestroyImage(device, image, nullptr);
		}
		for (const VkDeviceMemory memory : memories) {
			if (memory != VK_NULL_HANDLE)
				vkFreeMemory(device, memory, nullptr);
		}
		imageViews.clear();
		images.clear();
		memories.clear();
	}

	bool VulkanPostProcess::Initialize(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain, const VkFormat depthFormat) {
		Reset();
		device = sourceDevice.device;
		return CreateSceneImages(sourceDevice, sourceSwapChain)
			&& CreateDepthImages(sourceDevice, sourceSwapChain, depthFormat)
			&& CreateVelocityImages(sourceDevice)
			&& CreateEffectResources(sourceDevice)
			&& CreateFrameDataBuffers(sourceDevice)
			&& CreateDescriptors()
			&& CreatePipelines(sourceDevice);
	}
	
	bool VulkanPostProcess::BeginDepthPass(const VulkanCommandBuffer& commandBuffers,
		const uint32_t imageIndex, const VkPipeline geometryPipeline, const VkExtent2D extent) {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount)
			return false;
		constexpr bool depthHasStencil = false;
		const bool began = commandBuffers.BeginPostProcessDepthPass(imageIndex,
			sceneImages[imageIndex], depthImages[imageIndex], depthImageViews[imageIndex],
			RequiresVelocity() ? velocityImages[imageIndex] : VK_NULL_HANDLE,
			RequiresVelocity() ? velocityImageViews[imageIndex] : VK_NULL_HANDLE,
			RequiresVelocity() && velocityImageInitialized[imageIndex], depthHasStencil, geometryPipeline, extent);
		if (began && RequiresVelocity())
			velocityImageInitialized[imageIndex] = true;
		return began;
	}

	bool VulkanPostProcess::EndDepthPass(
		const VulkanCommandBuffer& commandBuffers, const uint32_t imageIndex) const {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount)
			return false;
		constexpr bool depthHasStencil = false;
		return commandBuffers.EndPostProcessDepthPass(imageIndex, depthImages[imageIndex],
			RequiresVelocity() ? velocityImages[imageIndex] : VK_NULL_HANDLE, depthHasStencil);
	}

	bool VulkanPostProcess::EndRecord(const VulkanCommandBuffer& commandBuffers, const uint32_t imageIndex,
		const VkImage swapChainImage, const VkImageView swapChainImageView, const VkExtent2D extent,
		const PostProcessFrameData& frameData, const bool sceneRenderingEnded) {
		const auto& routes = ResolvePassRoutes();
		if (imageIndex >= swapChainImageCount || imageIndex >= sceneImages.size()
			|| imageIndex >= frameDataDescriptorSets.size() || imageIndex >= frameDataBuffers.size()
			|| textureDescriptorSets.size() != swapChainImageCount * routes.size()
			|| pipelines.size() != routes.size() || pipelineLayout == VK_NULL_HANDLE)
			return false;
		if (!frameDataBuffers[imageIndex]->Write(&frameData, sizeof(frameData)))
			return false;
		const VkCommandBuffer commandBuffer = commandBuffers.ResolveCommandBuffer(imageIndex);
		if (commandBuffer == VK_NULL_HANDLE)
			return false;
		const VkDescriptorSet frameDataDescriptorSet = frameDataDescriptorSets[imageIndex];
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout, 0, 1, &frameDataDescriptorSet, 0, nullptr);
		if (!sceneRenderingEnded) {
			vkCmdEndRendering(commandBuffer);
			VulkanCommandBuffer::TransitionImage(commandBuffer, sceneImages[imageIndex],
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
		}
		constexpr VkImageSubresourceRange colorRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};
		constexpr VkClearColorValue clearColor{};
		const auto& resourcePlans = ResolveResourcePlans();
		for (size_t index = 0; index < resources.size() && index < resourcePlans.size(); index++) {
			VulkanPostProcessResource& resource = resources[index];
			if (resourcePlans[index].lifetime != EffectResourceLifetime::History || resource.historyInitialized)
				continue;
			for (const VkImage image : resource.images) {
				VulkanCommandBuffer::TransitionImage(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_IMAGE_ASPECT_COLOR_BIT);
				vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					&clearColor, 1, &colorRange);
				VulkanCommandBuffer::TransitionImage(commandBuffer, image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_IMAGE_ASPECT_COLOR_BIT);
			}
			resource.historyIndex = 0;
			resource.historyInitialized = true;
		}
		for (size_t passIndex = 0; passIndex < routes.size(); passIndex++) {
			if (pipelines[passIndex] == VK_NULL_HANDLE)
				return false;
			const PostProcessPassRoute& route = routes[passIndex];
			UpdateTextureDescriptorSet(imageIndex, passIndex);
			VkImage outputImage = VK_NULL_HANDLE;
			VkImageView outputImageView = VK_NULL_HANDLE;
			VkExtent2D outputExtent{};
			bool outputInitialized = false;
			if (!ResolveOutputImage(route, imageIndex, swapChainImage, swapChainImageView,
				outputImage, outputImageView, outputExtent, outputInitialized))
				return false;
			VulkanCommandBuffer::TransitionImage(commandBuffer, outputImage,
				outputInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				outputInitialized ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_2_NONE,
				outputInitialized ? VK_ACCESS_2_SHADER_SAMPLED_READ_BIT : VK_ACCESS_2_NONE,
				VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				VK_IMAGE_ASPECT_COLOR_BIT);
			const VkRenderingAttachmentInfo colorAttachment{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = outputImageView,
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE
			};
			const VkRenderingInfo renderingInfo{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = { .extent = outputExtent },
				.layerCount = 1,
				.colorAttachmentCount = 1,
				.pColorAttachments = &colorAttachment
			};
			const size_t descriptorIndex = ResolveTextureDescriptorIndex(imageIndex, passIndex);
			const VkDescriptorSet descriptorSet = textureDescriptorSets[descriptorIndex];
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[passIndex]);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 2, 1, &descriptorSet, 0, nullptr);
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
			vkCmdEndRendering(commandBuffer);
			if (route.outputKind == PostProcessOutputKind::Resource) {
				VulkanCommandBuffer::TransitionImage(commandBuffer, outputImage,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
					VK_IMAGE_ASPECT_COLOR_BIT);
			}
			AdvanceHistory(route);
		}
		VulkanCommandBuffer::TransitionImage(commandBuffer, swapChainImage,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_ASPECT_COLOR_BIT);
		if (vkEndCommandBuffer(commandBuffer) == VK_SUCCESS)
			return true;
		std::cerr << "Failed to record Vulkan post-process command buffer.\n";
		return false;
	}

	void VulkanPostProcess::ResetHistory() {
		for (auto& resource : resources) {
			resource.historyIndex = 0;
			resource.historyInitialized = false;
		}
	}

	void VulkanPostProcess::Reset() {
		if (device == VK_NULL_HANDLE)
			return;
		for (const VkPipeline pipeline : pipelines) {
			if (pipeline != VK_NULL_HANDLE)
				vkDestroyPipeline(device, pipeline, nullptr);
		}
		pipelines.clear();
		if (pipelineLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		if (descriptorPool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		if (sampler != VK_NULL_HANDLE)
			vkDestroySampler(device, sampler, nullptr);
		for (VkDescriptorSetLayout& layout : descriptorSetLayouts) {
			if (layout != VK_NULL_HANDLE)
				vkDestroyDescriptorSetLayout(device, layout, nullptr);
			layout = VK_NULL_HANDLE;
		}
		for (auto& resource : resources)
			DestroyImages(resource.images, resource.memories, resource.imageViews);
		resources.clear();
		DestroyImages(sceneImages, sceneImageMemories, sceneImageViews);
		DestroyImages(depthImages, depthImageMemories, depthImageViews);
		DestroyImages(velocityImages, velocityImageMemories, velocityImageViews);
		velocityImageInitialized.clear();
		frameDataBuffers.clear();
		frameDataDescriptorSets.clear();
		textureDescriptorSets.clear();
		descriptorPool = VK_NULL_HANDLE;
		pipelineLayout = VK_NULL_HANDLE;
		sampler = VK_NULL_HANDLE;
		swapChainImageCount = 0;
		targetExtent = {};
		swapChainFormat = VK_FORMAT_UNDEFINED;
		device = VK_NULL_HANDLE;
	}
}
