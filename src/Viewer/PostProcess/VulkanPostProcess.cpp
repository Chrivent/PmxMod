#include "Viewer/PostProcess/VulkanPostProcess.h"

#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Memory/VulkanMemory.h"
#include "Viewer/Shader/ModernHlslCompiler.h"
#include "Viewer/Shader/VulkanShaderModule.h"
#include "Viewer/Viewer/Viewer.h"
#include "Viewer/Command/VulkanCommandBuffer.h"

#include <iostream>
#include <utility>

namespace Chrivent {
	VulkanPostProcess::~VulkanPostProcess() {
		VulkanPostProcess::ResetResources();
	}

	bool VulkanPostProcess::CreateSceneImages(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		swapChainImageCount = sourceSwapChain.images.size();
		targetExtent = sourceSwapChain.extent;
		swapChainFormat = sourceSwapChain.imageFormat;
		return sceneTarget.Initialize(sourceDevice, swapChainImageCount, targetExtent, swapChainFormat,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
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
		return velocityTarget.Initialize(sourceDevice, swapChainImageCount, targetExtent,
			VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, true);
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
			const size_t imageCount = plan.lifetime == EffectResourceLifetime::History ? 2 : swapChainImageCount;
			const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
				| (plan.lifetime == EffectResourceLifetime::History ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);
			if (!resources[resourceIndex].Initialize(sourceDevice, imageCount, ResolveResourceExtent(plan),
				ResolveResourceFormat(plan), usage, plan.lifetime == EffectResourceLifetime::Transient))
				return false;
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

	bool VulkanPostProcess::CreateParameterDataBuffers(const VulkanDevice& sourceDevice) {
		parameterDataBuffers.clear();
		const size_t bufferCount = swapChainImageCount * ResolvePassRoutes().size();
		for (size_t index = 0; index < bufferCount; index++) {
			auto buffer = std::make_unique<VulkanBuffer>();
			if (!buffer->Initialize(sourceDevice, sizeof(PostProcessParameterData),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				return false;
			parameterDataBuffers.push_back(std::move(buffer));
		}
		return true;
	}

	bool VulkanPostProcess::CreateDescriptors() {
		constexpr VkDescriptorSetLayoutBinding frameDataBinding{
			.binding = PostProcessInputLayout::frameDataVulkanBinding,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		VkDescriptorSetLayoutCreateInfo frameLayoutInfo{};
		frameLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		frameLayoutInfo.bindingCount = 1;
		frameLayoutInfo.pBindings = &frameDataBinding;
		constexpr VkDescriptorSetLayoutBinding parameterDataBinding{
			.binding = PostProcessInputLayout::parameterDataVulkanBinding,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		VkDescriptorSetLayoutCreateInfo parameterLayoutInfo{};
		parameterLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		parameterLayoutInfo.bindingCount = 1;
		parameterLayoutInfo.pBindings = &parameterDataBinding;
		if (vkCreateDescriptorSetLayout(device, &frameLayoutInfo, nullptr,
			&descriptorSetLayouts[PostProcessInputLayout::frameDataVulkanSet]) != VK_SUCCESS
			|| vkCreateDescriptorSetLayout(device, &parameterLayoutInfo, nullptr,
				&descriptorSetLayouts[PostProcessInputLayout::parameterDataVulkanSet]) != VK_SUCCESS)
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
		for (uint32_t samplerSlot = 0; samplerSlot < PostProcessInputLayout::samplerCount; samplerSlot++) {
			textureBindings.emplace_back(VkDescriptorSetLayoutBinding{
				.binding = PostProcessInputLayout::ResolveSpirvSamplerBinding(samplerSlot),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			});
		}
		VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
		textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		textureLayoutInfo.bindingCount = static_cast<uint32_t>(textureBindings.size());
		textureLayoutInfo.pBindings = textureBindings.data();
		if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr,
			&descriptorSetLayouts[PostProcessInputLayout::textureVulkanSet]) != VK_SUCCESS)
			return false;
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = PostProcessInputLayout::vulkanSetCount;
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
		const uint32_t parameterSetCount = textureSetCount;
		const VkDescriptorPoolSize poolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameSetCount + parameterSetCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, textureSetCount * PostProcessInputLayout::maxTextureCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLER, textureSetCount * PostProcessInputLayout::samplerCount }
		};
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = frameSetCount + parameterSetCount + textureSetCount;
		poolInfo.poolSizeCount = 3;
		poolInfo.pPoolSizes = poolSizes;
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			return false;
		std::vector layouts(frameSetCount + parameterSetCount + textureSetCount,
			descriptorSetLayouts[PostProcessInputLayout::textureVulkanSet]);
		for (uint32_t index = 0; index < frameSetCount; index++)
			layouts[index] = descriptorSetLayouts[PostProcessInputLayout::frameDataVulkanSet];
		for (uint32_t index = frameSetCount; index < frameSetCount + parameterSetCount; index++)
			layouts[index] = descriptorSetLayouts[PostProcessInputLayout::parameterDataVulkanSet];
		std::vector<VkDescriptorSet> sets(layouts.size());
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
		allocateInfo.pSetLayouts = layouts.data();
		if (vkAllocateDescriptorSets(device, &allocateInfo, sets.data()) != VK_SUCCESS)
			return false;
		frameDataDescriptorSets.assign(sets.begin(), sets.begin() + frameSetCount);
		parameterDataDescriptorSets.assign(
			sets.begin() + frameSetCount, sets.begin() + frameSetCount + parameterSetCount);
		textureDescriptorSets.assign(sets.begin() + frameSetCount + parameterSetCount, sets.end());
		for (uint32_t index = 0; index < frameSetCount; index++) {
			const VkDescriptorBufferInfo bufferInfo{
				.buffer = frameDataBuffers[index]->buffer,
				.offset = 0,
				.range = sizeof(PostProcessFrameData)
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frameDataDescriptorSets[index],
				.dstBinding = PostProcessInputLayout::frameDataVulkanBinding,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			};
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
		for (uint32_t index = 0; index < parameterSetCount; index++) {
			const VkDescriptorBufferInfo bufferInfo{
				.buffer = parameterDataBuffers[index]->buffer,
				.offset = 0,
				.range = sizeof(PostProcessParameterData)
			};
			const VkWriteDescriptorSet write{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = parameterDataDescriptorSets[index],
				.dstBinding = PostProcessInputLayout::parameterDataVulkanBinding,
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
			return sceneTarget.ResolveImageView(imageIndex);
		if (input.kind == PostProcessInputKind::SceneDepth)
			return imageIndex < depthImageViews.size() ? depthImageViews[imageIndex] : VK_NULL_HANDLE;
		if (input.kind == PostProcessInputKind::SceneVelocity)
			return velocityTarget.ResolveImageView(imageIndex);
		if (input.resourceIndex >= resources.size())
			return VK_NULL_HANDLE;
		const VulkanPostProcessTarget& resource = resources[input.resourceIndex];
		const size_t index = ResolveResourceReadIndex(input.resourceIndex, imageIndex);
		return resource.ResolveImageView(index);
	}

	void VulkanPostProcess::UpdateTextureDescriptorSet(
		const uint32_t imageIndex, const size_t passIndex) const {
		const size_t descriptorIndex = ResolveTextureDescriptorIndex(imageIndex, passIndex);
		if (descriptorIndex >= textureDescriptorSets.size() || passIndex >= ResolvePassRoutes().size())
			return;
		std::vector<const PostProcessPassInputRoute*> slots(PostProcessInputLayout::maxTextureCount);
		for (const auto& input : ResolvePassRoutes()[passIndex].inputs)
			slots[input.slot] = &input;
		std::vector<VkDescriptorImageInfo> imageInfos(
			PostProcessInputLayout::maxTextureCount + PostProcessInputLayout::samplerCount);
		std::vector<VkWriteDescriptorSet> writes;
		writes.reserve(imageInfos.size());
		for (uint32_t slot = 0; slot < PostProcessInputLayout::maxTextureCount; slot++) {
			VkImageView imageView = sceneTarget.ResolveImageView(imageIndex);
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
		for (uint32_t samplerSlot = 0; samplerSlot < PostProcessInputLayout::samplerCount; samplerSlot++) {
			const size_t infoIndex = PostProcessInputLayout::maxTextureCount + samplerSlot;
			imageInfos[infoIndex] = { .sampler = sampler };
			writes.emplace_back(VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = textureDescriptorSets[descriptorIndex],
				.dstBinding = PostProcessInputLayout::ResolveSpirvSamplerBinding(samplerSlot),
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
		if (!ModernHlslCompiler::CompileSpirv(pass.shaderPath, vertexEntry, L"vs_6_0", SpirvTarget::Vulkan,
			SpirvBindingProfile::PostProcess, vertexCode, error, true)
			|| !ModernHlslCompiler::CompileSpirv(pass.shaderPath, pixelEntry, L"ps_6_0", SpirvTarget::Vulkan,
				SpirvBindingProfile::PostProcess, pixelCode, error)) {
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
		VulkanPostProcessTarget& resource = resources[route.outputResourceIndex];
		const PostProcessResourcePlan& plan = ResolveResourcePlans()[route.outputResourceIndex];
		const size_t index = ResolveResourceWriteIndex(route.outputResourceIndex, imageIndex);
		if (index >= resource.GetImageCount())
			return false;
		image = resource.ResolveImage(index);
		imageView = resource.ResolveImageView(index);
		extent = ResolveResourceExtent(plan);
		initialized = plan.lifetime == EffectResourceLifetime::History
			? !NeedsHistoryInitialization(route.outputResourceIndex) : resource.IsInitialized(index);
		if (plan.lifetime == EffectResourceLifetime::Transient)
			resource.MarkInitialized(index);
		return true;
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

	void VulkanPostProcess::SwapResources(VulkanPostProcess& other) noexcept {
		std::swap(device, other.device);
		std::swap(targetExtent, other.targetExtent);
		std::swap(swapChainFormat, other.swapChainFormat);
		std::swap(sceneTarget, other.sceneTarget);
		depthImages.swap(other.depthImages);
		depthImageMemories.swap(other.depthImageMemories);
		depthImageViews.swap(other.depthImageViews);
		std::swap(velocityTarget, other.velocityTarget);
		resources.swap(other.resources);
		textureDescriptorSets.swap(other.textureDescriptorSets);
		frameDataDescriptorSets.swap(other.frameDataDescriptorSets);
		parameterDataDescriptorSets.swap(other.parameterDataDescriptorSets);
		frameDataBuffers.swap(other.frameDataBuffers);
		parameterDataBuffers.swap(other.parameterDataBuffers);
		for (size_t index = 0; index < PostProcessInputLayout::vulkanSetCount; index++)
			std::swap(descriptorSetLayouts[index], other.descriptorSetLayouts[index]);
		std::swap(descriptorPool, other.descriptorPool);
		std::swap(pipelineLayout, other.pipelineLayout);
		pipelines.swap(other.pipelines);
		std::swap(sampler, other.sampler);
		std::swap(swapChainImageCount, other.swapChainImageCount);
	}

	bool VulkanPostProcess::Initialize(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain, const VkFormat depthFormat) {
		ResetResources();
		device = sourceDevice.device;
		return CreateSceneImages(sourceDevice, sourceSwapChain)
			&& CreateDepthImages(sourceDevice, sourceSwapChain, depthFormat)
			&& CreateVelocityImages(sourceDevice)
			&& CreateEffectResources(sourceDevice)
			&& CreateFrameDataBuffers(sourceDevice)
			&& CreateParameterDataBuffers(sourceDevice)
			&& CreateDescriptors()
			&& CreatePipelines(sourceDevice);
	}

	bool VulkanPostProcess::Load(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat, const std::vector<const EffectDefinition*>& effects) {
		VulkanPostProcess candidate;
		if (!candidate.SetEffects(effects)
			|| (candidate.HasEffects() && !candidate.Initialize(sourceDevice, sourceSwapChain, depthFormat)))
			return false;
		SwapExecutionPlan(candidate);
		SwapResources(candidate);
		ResetHistory();
		return true;
	}
	
	bool VulkanPostProcess::BeginSceneInputPass(const VulkanCommandBuffer& commandBuffers,
		const uint32_t imageIndex, const VkPipeline geometryPipeline, const VkExtent2D extent) {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount)
			return false;
		constexpr bool depthHasStencil = false;
		const bool began = commandBuffers.BeginPostProcessSceneInputPass(imageIndex,
			sceneTarget.ResolveImage(imageIndex), depthImages[imageIndex], depthImageViews[imageIndex],
			RequiresVelocity() ? velocityTarget.ResolveImage(imageIndex) : VK_NULL_HANDLE,
			RequiresVelocity() ? velocityTarget.ResolveImageView(imageIndex) : VK_NULL_HANDLE,
			RequiresVelocity() && velocityTarget.IsInitialized(imageIndex), depthHasStencil, geometryPipeline, extent);
		if (began && RequiresVelocity())
			velocityTarget.MarkInitialized(imageIndex);
		return began;
	}

	bool VulkanPostProcess::EndSceneInputPass(
		const VulkanCommandBuffer& commandBuffers, const uint32_t imageIndex) const {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount)
			return false;
		constexpr bool depthHasStencil = false;
		return commandBuffers.EndPostProcessSceneInputPass(imageIndex, depthImages[imageIndex],
			RequiresVelocity() ? velocityTarget.ResolveImage(imageIndex) : VK_NULL_HANDLE, depthHasStencil);
	}

	bool VulkanPostProcess::EndRecord(const VulkanCommandBuffer& commandBuffers, const uint32_t imageIndex,
		const VkImage swapChainImage, const VkImageView swapChainImageView, const VkExtent2D extent,
		const PostProcessFrameData& frameData, const bool sceneRenderingEnded) {
		const auto& routes = ResolvePassRoutes();
		if (imageIndex >= swapChainImageCount || imageIndex >= sceneTarget.GetImageCount()
			|| imageIndex >= frameDataDescriptorSets.size() || imageIndex >= frameDataBuffers.size()
			|| parameterDataDescriptorSets.size() != swapChainImageCount * routes.size()
			|| parameterDataBuffers.size() != swapChainImageCount * routes.size()
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
		BeginHistoryFrame();
		if (!sceneRenderingEnded) {
			vkCmdEndRendering(commandBuffer);
			VulkanCommandBuffer::TransitionImage(commandBuffer, sceneTarget.ResolveImage(imageIndex),
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
			const VulkanPostProcessTarget& resource = resources[index];
			if (!NeedsHistoryInitialization(index))
				continue;
			for (const VkImage image : resource.GetImages()) {
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
			MarkHistoryInitialized(index);
		}
		for (size_t passIndex = 0; passIndex < routes.size(); passIndex++) {
			if (pipelines[passIndex] == VK_NULL_HANDLE) {
				DiscardHistoryFrame();
				return false;
			}
			const PostProcessPassRoute& route = routes[passIndex];
			const size_t parameterIndex = ResolveTextureDescriptorIndex(imageIndex, passIndex);
			if (!parameterDataBuffers[parameterIndex]->Write(&route.parameters, sizeof(route.parameters))) {
				DiscardHistoryFrame();
				return false;
			}
			UpdateTextureDescriptorSet(imageIndex, passIndex);
			VkImage outputImage = VK_NULL_HANDLE;
			VkImageView outputImageView = VK_NULL_HANDLE;
			VkExtent2D outputExtent{};
			bool outputInitialized = false;
			if (!ResolveOutputImage(route, imageIndex, swapChainImage, swapChainImageView,
				outputImage, outputImageView, outputExtent, outputInitialized)) {
				DiscardHistoryFrame();
				return false;
			}
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
			const VkDescriptorSet parameterSet = parameterDataDescriptorSets[parameterIndex];
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 1, 1, &parameterSet, 0, nullptr);
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
		DiscardHistoryFrame();
		std::cerr << "Failed to record Vulkan post-process command buffer.\n";
		return false;
	}

	void VulkanPostProcess::ResetResources() {
		if (device != VK_NULL_HANDLE) {
			for (const VkPipeline pipeline : pipelines) {
				if (pipeline != VK_NULL_HANDLE)
					vkDestroyPipeline(device, pipeline, nullptr);
			}
			if (pipelineLayout != VK_NULL_HANDLE)
				vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			if (descriptorPool != VK_NULL_HANDLE)
				vkDestroyDescriptorPool(device, descriptorPool, nullptr);
			if (sampler != VK_NULL_HANDLE)
				vkDestroySampler(device, sampler, nullptr);
			for (const VkDescriptorSetLayout& layout : descriptorSetLayouts) {
				if (layout != VK_NULL_HANDLE)
					vkDestroyDescriptorSetLayout(device, layout, nullptr);
			}
			DestroyImages(depthImages, depthImageMemories, depthImageViews);
		}
		pipelines.clear();
		sceneTarget.Reset();
		velocityTarget.Reset();
		resources.clear();
		depthImages.clear();
		depthImageMemories.clear();
		depthImageViews.clear();
		frameDataBuffers.clear();
		parameterDataBuffers.clear();
		frameDataDescriptorSets.clear();
		parameterDataDescriptorSets.clear();
		textureDescriptorSets.clear();
		descriptorPool = VK_NULL_HANDLE;
		pipelineLayout = VK_NULL_HANDLE;
		sampler = VK_NULL_HANDLE;
		for (VkDescriptorSetLayout& layout : descriptorSetLayouts)
			layout = VK_NULL_HANDLE;
		swapChainImageCount = 0;
		targetExtent = {};
		swapChainFormat = VK_FORMAT_UNDEFINED;
		device = VK_NULL_HANDLE;
	}
}
