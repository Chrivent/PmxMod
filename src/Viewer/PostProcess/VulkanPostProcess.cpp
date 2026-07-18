#include "Viewer/PostProcess/VulkanPostProcess.h"

#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Memory/VulkanMemory.h"
#include "Viewer/Command/VulkanCommandBuffer.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Chrivent {
	VulkanPostProcess::~VulkanPostProcess() {
		VulkanPostProcess::ResetResources();
	}

	bool VulkanPostProcess::CreateSceneImages(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		swapChainImageCount = sourceSwapChain.GetImageCount();
		targetExtent = sourceSwapChain.GetExtent();
		swapChainFormat = sourceSwapChain.GetImageFormat();
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
			imageInfo.extent = {
				sourceSwapChain.GetExtent().width,
				sourceSwapChain.GetExtent().height,
				1
			};
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
		const auto& plans = GetResourcePlans();
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
		const size_t passCount = GetPassRoutes().size();
		const VkDeviceSize alignment = std::max<VkDeviceSize>(
			1, sourceDevice.properties.limits.minUniformBufferOffsetAlignment);
		constexpr VkDeviceSize parameterSize = sizeof(PostProcessParameterData);
		parameterDataStride = (parameterSize + alignment - 1) / alignment * alignment;
		if (passCount == 0 || passCount > std::numeric_limits<VkDeviceSize>::max() / parameterDataStride)
			return false;
		const VkDeviceSize bufferSize = passCount * parameterDataStride;
		for (size_t index = 0; index < swapChainImageCount; index++) {
			auto buffer = std::make_unique<VulkanBuffer>();
			if (!buffer->Initialize(sourceDevice, bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
				return false;
			parameterDataBuffers.push_back(std::move(buffer));
		}
		return true;
	}

	VkImageView VulkanPostProcess::ResolveInputImageView(
		const PostProcessPassInputRoute& input, const uint32_t imageIndex) const {
		if (input.kind == PostProcessInputKind::SceneColor)
			return sceneTarget.TryGetImageView(imageIndex);
		if (input.kind == PostProcessInputKind::SceneDepth)
			return imageIndex < depthImageViews.size() ? depthImageViews[imageIndex] : VK_NULL_HANDLE;
		if (input.kind == PostProcessInputKind::SceneVelocity)
			return velocityTarget.TryGetImageView(imageIndex);
		if (input.resourceIndex >= resources.size())
			return VK_NULL_HANDLE;
		const VulkanPostProcessTarget& resource = resources[input.resourceIndex];
		const size_t index = ResolveResourceReadIndex(input.resourceIndex, imageIndex);
		return resource.TryGetImageView(index);
	}

	bool VulkanPostProcess::UpdateTextureDescriptorSet(
		const uint32_t imageIndex, const size_t passIndex) {
		const auto& routes = GetPassRoutes();
		if (passIndex >= routes.size())
			return false;
		VkImageView imageViews[PostProcessInputLayout::maxTextureCount]{};
		for (VkImageView& imageView : imageViews)
			imageView = sceneTarget.TryGetImageView(imageIndex);
		for (const auto& input : routes[passIndex].inputs)
			imageViews[input.slot] = ResolveInputImageView(input, imageIndex);
		return descriptors.UpdateTextures(imageIndex, passIndex, imageViews);
	}

	bool VulkanPostProcess::CreatePipelines(const VulkanDevice& sourceDevice) {
		const auto& passes = GetShaderPrograms();
		const auto& routes = GetPassRoutes();
		std::vector<VkFormat> targetFormats;
		targetFormats.reserve(passes.size());
		for (size_t index = 0; index < passes.size(); index++) {
			VkFormat format = swapChainFormat;
			if (routes[index].outputKind == PostProcessOutputKind::Resource) {
				const PostProcessResourcePlan& resource = GetResourcePlans()[routes[index].outputResourceIndex];
				format = ResolveResourceFormat(resource);
			}
			targetFormats.push_back(format);
		}
		return pipelines.Initialize(sourceDevice,
			descriptors.GetPipelineLayout(), passes, targetFormats);
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
		const PostProcessResourcePlan& plan = GetResourcePlans()[route.outputResourceIndex];
		const size_t index = ResolveResourceWriteIndex(route.outputResourceIndex, imageIndex);
		if (index >= resource.GetImageCount())
			return false;
		image = resource.TryGetImage(index);
		imageView = resource.TryGetImageView(index);
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
		frameDataBuffers.swap(other.frameDataBuffers);
		parameterDataBuffers.swap(other.parameterDataBuffers);
		descriptors.Swap(other.descriptors);
		pipelines.Swap(other.pipelines);
		std::swap(swapChainImageCount, other.swapChainImageCount);
		std::swap(parameterDataStride, other.parameterDataStride);
	}

	bool VulkanPostProcess::InitializeTargets(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain, const VkFormat depthFormat) {
		ResetResources();
		device = sourceDevice.device;
		return CreateSceneImages(sourceDevice, sourceSwapChain)
			&& (!(RequiresDepth() || RequiresVelocity())
				|| CreateDepthImages(sourceDevice, sourceSwapChain, depthFormat))
			&& (!RequiresVelocity() || CreateVelocityImages(sourceDevice))
			&& CreateEffectResources(sourceDevice)
			&& CreateFrameDataBuffers(sourceDevice)
			&& CreateParameterDataBuffers(sourceDevice)
			&& descriptors.Initialize(device, swapChainImageCount, GetPassRoutes().size(),
				frameDataBuffers, parameterDataBuffers,
				sizeof(PostProcessFrameData), sizeof(PostProcessParameterData), parameterDataStride)
			&& CreatePipelines(sourceDevice);
	}

	bool VulkanPostProcess::Configure(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat, const std::vector<const EffectRuntimeDefinition*>& effects) {
		VulkanPostProcess candidate;
		if (!candidate.SetEffects(effects)
			|| (candidate.HasEffects() && !candidate.InitializeTargets(sourceDevice, sourceSwapChain, depthFormat)))
			return false;
		SwapExecutionPlan(candidate);
		SwapResources(candidate);
		return true;
	}
	
	bool VulkanPostProcess::BeginSceneInputPass(const VulkanCommandBuffer& commandBuffers,
		const uint32_t imageIndex, const VkExtent2D extent) {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount)
			return false;
		constexpr bool depthHasStencil = false;
		const bool began = commandBuffers.BeginPostProcessSceneInputPass(imageIndex,
			sceneTarget.TryGetImage(imageIndex), depthImages[imageIndex], depthImageViews[imageIndex],
			RequiresVelocity() ? velocityTarget.TryGetImage(imageIndex) : VK_NULL_HANDLE,
			RequiresVelocity() ? velocityTarget.TryGetImageView(imageIndex) : VK_NULL_HANDLE,
			RequiresVelocity() && velocityTarget.IsInitialized(imageIndex), depthHasStencil, extent);
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
			RequiresVelocity() ? velocityTarget.TryGetImage(imageIndex) : VK_NULL_HANDLE, depthHasStencil);
	}

	bool VulkanPostProcess::Draw(const VulkanCommandBuffer& commandBuffers, const uint32_t imageIndex,
		const VkImage swapChainImage, const VkImageView swapChainImageView,
		const PostProcessFrameData& frameData) {
		const auto& routes = GetPassRoutes();
		if (imageIndex >= swapChainImageCount || imageIndex >= sceneTarget.GetImageCount()
			|| imageIndex >= frameDataBuffers.size()
			|| parameterDataBuffers.size() != swapChainImageCount
			|| !IsPassCountCompatible(pipelines.GetCount())
			|| !descriptors.IsCompatible(swapChainImageCount, routes.size()))
			return false;
		if (!frameDataBuffers[imageIndex]->Write(&frameData, sizeof(frameData)))
			return false;
		const VkCommandBuffer commandBuffer = commandBuffers.TryGetCommandBuffer(imageIndex);
		if (commandBuffer == VK_NULL_HANDLE)
			return false;
		const VkPipelineLayout pipelineLayout = descriptors.GetPipelineLayout();
		const VkDescriptorSet frameDataDescriptorSet = descriptors.GetFrameDataDescriptorSet(imageIndex);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout, 0, 1, &frameDataDescriptorSet, 0, nullptr);
		BeginHistoryFrame();
		constexpr VkImageSubresourceRange colorRange{
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
		};
		constexpr VkClearColorValue clearColor{};
		const auto& resourcePlans = GetResourcePlans();
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
			if (pipelines.TryGetPipeline(passIndex) == VK_NULL_HANDLE) {
				DiscardHistoryFrame();
				return false;
			}
			const PostProcessPassRoute& route = routes[passIndex];
			const PostProcessParameterData& parameterData = GetParameterData(route);
			if (!parameterDataBuffers[imageIndex]->Write(
				&parameterData, sizeof(parameterData), passIndex * parameterDataStride)) {
				DiscardHistoryFrame();
				return false;
			}
			if (!UpdateTextureDescriptorSet(imageIndex, passIndex)) {
				DiscardHistoryFrame();
				return false;
			}
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
			const VkDescriptorSet descriptorSet = descriptors.GetTextureDescriptorSet(imageIndex, passIndex);
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			VulkanCommandBuffer::ApplyViewportAndScissor(commandBuffer, outputExtent);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.TryGetPipeline(passIndex));
			const VkDescriptorSet parameterSet = descriptors.GetParameterDataDescriptorSet(imageIndex, passIndex);
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
		return true;
	}

	void VulkanPostProcess::ResetResources() {
		if (device != VK_NULL_HANDLE)
			DestroyImages(depthImages, depthImageMemories, depthImageViews);
		pipelines.Reset();
		descriptors.Reset();
		sceneTarget.Reset();
		velocityTarget.Reset();
		resources.clear();
		depthImages.clear();
		depthImageMemories.clear();
		depthImageViews.clear();
		frameDataBuffers.clear();
		parameterDataBuffers.clear();
		swapChainImageCount = 0;
		parameterDataStride = 0;
		targetExtent = {};
		swapChainFormat = VK_FORMAT_UNDEFINED;
		device = VK_NULL_HANDLE;
	}
}
