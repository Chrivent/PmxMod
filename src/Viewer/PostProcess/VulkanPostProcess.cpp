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

	GraphicsResult<void> VulkanPostProcess::CreateSceneImages(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		swapChainImageCount = sourceSwapChain.GetImageCount();
		targetExtent = sourceSwapChain.GetExtent();
		swapChainFormat = sourceSwapChain.GetImageFormat();
		return sceneTarget.Initialize(sourceDevice, swapChainImageCount, targetExtent, swapChainFormat,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
	}

	GraphicsResult<void> VulkanPostProcess::CreateDepthImages(const VulkanDevice& sourceDevice,
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
			VkResult result = vkCreateImage(device, &imageInfo, nullptr, &depthImages[index]);
			if (result != VK_SUCCESS) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 depth image 생성",
					"Vulkan 후처리 depth image를 만들지 못했습니다", result, true));
			}
			VkMemoryRequirements requirements{};
			vkGetImageMemoryRequirements(device, depthImages[index], &requirements);
			uint32_t memoryType = 0;
			if (!VulkanMemory::FindMemoryType(
				sourceDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType)) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::UnsupportedFeature, "후처리 depth memory type 선택",
					"Vulkan 후처리 depth image에 사용할 memory type을 찾지 못했습니다"));
			}
			VkMemoryAllocateInfo allocateInfo{};
			allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocateInfo.allocationSize = requirements.size;
			allocateInfo.memoryTypeIndex = memoryType;
			result = vkAllocateMemory(device, &allocateInfo, nullptr, &depthImageMemories[index]);
			if (result != VK_SUCCESS) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 depth memory 할당",
					"Vulkan 후처리 depth image memory를 할당하지 못했습니다", result, true));
			}
			result = vkBindImageMemory(device, depthImages[index], depthImageMemories[index], 0);
			if (result != VK_SUCCESS) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 depth memory 연결",
					"Vulkan 후처리 depth image memory를 연결하지 못했습니다", result, true));
			}
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = depthImages[index];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = depthFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.layerCount = 1;
			result = vkCreateImageView(device, &viewInfo, nullptr, &depthImageViews[index]);
			if (result != VK_SUCCESS) {
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ResourceCreationFailed, "후처리 depth image view 생성",
					"Vulkan 후처리 depth image view를 만들지 못했습니다", result, true));
			}
		}
		return {};
	}

	GraphicsResult<void> VulkanPostProcess::CreateVelocityImages(const VulkanDevice& sourceDevice) {
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

	GraphicsResult<void> VulkanPostProcess::CreateEffectResources(const VulkanDevice& sourceDevice) {
		const auto& plans = GetResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const PostProcessResourcePlan& plan = plans[resourceIndex];
			const size_t imageCount = plan.lifetime == EffectResourceLifetime::History ? 2 : swapChainImageCount;
			const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
				| (plan.lifetime == EffectResourceLifetime::History ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0);
			const auto result = resources[resourceIndex].Initialize(sourceDevice, imageCount,
				ResolveResourceExtent(plan), ResolveResourceFormat(plan), usage,
				plan.lifetime == EffectResourceLifetime::Transient);
			if (!result)
				return std::unexpected(result.error());
		}
		ResetHistory();
		return {};
	}

	GraphicsResult<void> VulkanPostProcess::CreateFrameDataBuffers(const VulkanDevice& sourceDevice) {
		frameDataBuffers.clear();
		for (size_t index = 0; index < swapChainImageCount; index++) {
			auto buffer = std::make_unique<VulkanBuffer>();
			const auto result = buffer->Initialize(sourceDevice, sizeof(PostProcessFrameData),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			if (!result)
				return std::unexpected(result.error());
			frameDataBuffers.push_back(std::move(buffer));
		}
		return {};
	}

	GraphicsResult<void> VulkanPostProcess::CreateParameterDataBuffers(const VulkanDevice& sourceDevice) {
		parameterDataBuffers.clear();
		const size_t passCount = GetPassRoutes().size();
		const VkDeviceSize alignment = std::max<VkDeviceSize>(
			1, sourceDevice.GetUniformBufferAlignment());
		constexpr VkDeviceSize parameterSize = sizeof(PostProcessParameterData);
		parameterDataStride = (parameterSize + alignment - 1) / alignment * alignment;
		if (passCount == 0 || passCount > std::numeric_limits<VkDeviceSize>::max() / parameterDataStride) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "후처리 parameter buffer 크기 계산",
				"Vulkan 후처리 패스 수가 parameter buffer 크기 한도를 넘습니다"));
		}
		const VkDeviceSize bufferSize = passCount * parameterDataStride;
		for (size_t index = 0; index < swapChainImageCount; index++) {
			auto buffer = std::make_unique<VulkanBuffer>();
			const auto result = buffer->Initialize(sourceDevice, bufferSize,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			if (!result)
				return std::unexpected(result.error());
			parameterDataBuffers.push_back(std::move(buffer));
		}
		return {};
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

	void VulkanPostProcess::BeginImageStateFrame() {
		velocityTarget.BeginInitializationFrame();
		for (auto& resource : resources)
			resource.BeginInitializationFrame();
	}

	GraphicsResult<void> VulkanPostProcess::CreatePipelines(const VulkanDevice& sourceDevice) {
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
		std::string error;
		if (!pipelines.Initialize(sourceDevice,
			descriptors.GetPipelineLayout(), passes, targetFormats, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::EffectConfigurationFailed, "후처리 pipeline 생성",
				error.empty() ? "Vulkan 후처리 pipeline을 만들지 못했습니다" : std::move(error)));
		}
		return {};
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

	GraphicsResult<void> VulkanPostProcess::InitializeTargets(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat) {
		ResetResources();
		device = sourceDevice.GetDevice();
		auto result = CreateSceneImages(sourceDevice, sourceSwapChain);
		if (result && (RequiresDepth() || RequiresVelocity()))
			result = CreateDepthImages(sourceDevice, sourceSwapChain, depthFormat);
		if (result && RequiresVelocity())
			result = CreateVelocityImages(sourceDevice);
		if (result)
			result = CreateEffectResources(sourceDevice);
		if (result)
			result = CreateFrameDataBuffers(sourceDevice);
		if (result)
			result = CreateParameterDataBuffers(sourceDevice);
		if (result) {
			result = descriptors.Initialize(device, swapChainImageCount, GetPassRoutes().size(),
				frameDataBuffers, parameterDataBuffers, sizeof(PostProcessFrameData),
				sizeof(PostProcessParameterData), parameterDataStride);
		}
		if (result)
			result = CreatePipelines(sourceDevice);
		if (result)
			return {};
		const GraphicsError error = result.error();
		ResetResources();
		return std::unexpected(error);
	}

	GraphicsResult<void> VulkanPostProcess::Configure(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat, const std::vector<const EffectRuntimeDefinition*>& effects) {
		VulkanPostProcess candidate;
		const auto planResult = candidate.SetEffects(effects);
		if (!planResult) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "후처리 실행 계획 생성", planResult.error()));
		}
		if (candidate.HasEffects()) {
			const auto initializeResult = candidate.InitializeTargets(
				sourceDevice, sourceSwapChain, depthFormat);
			if (!initializeResult)
				return std::unexpected(initializeResult.error());
		}
		SwapExecutionPlan(candidate);
		SwapResources(candidate);
		return {};
	}
	
	GraphicsResult<void> VulkanPostProcess::BeginSceneInputPass(const VulkanCommandBuffer& commandBuffers,
		const uint32_t imageIndex, const VkExtent2D extent) {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 시작",
				"Vulkan 후처리 장면 입력 target 또는 image index가 올바르지 않습니다"));
		}
		BeginImageStateFrame();
		constexpr bool depthHasStencil = false;
		const bool began = commandBuffers.BeginPostProcessSceneInputPass(imageIndex,
			sceneTarget.TryGetImage(imageIndex), depthImages[imageIndex], depthImageViews[imageIndex],
			RequiresVelocity() ? velocityTarget.TryGetImage(imageIndex) : VK_NULL_HANDLE,
			RequiresVelocity() ? velocityTarget.TryGetImageView(imageIndex) : VK_NULL_HANDLE,
			RequiresVelocity() && velocityTarget.IsInitialized(imageIndex), depthHasStencil, extent);
		if (!began) {
			DiscardImageStateFrame();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "후처리 장면 입력 패스 시작",
				"Vulkan 장면 입력 패스를 시작하지 못했습니다"));
		}
		if (RequiresVelocity())
			velocityTarget.MarkInitialized(imageIndex);
		return {};
	}

	GraphicsResult<void> VulkanPostProcess::EndSceneInputPass(
		const VulkanCommandBuffer& commandBuffers, const uint32_t imageIndex) {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount) {
			DiscardImageStateFrame();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 종료",
				"Vulkan 후처리 장면 입력 target 또는 image index가 올바르지 않습니다"));
		}
		constexpr bool depthHasStencil = false;
		if (!commandBuffers.EndPostProcessSceneInputPass(imageIndex, depthImages[imageIndex],
			RequiresVelocity() ? velocityTarget.TryGetImage(imageIndex) : VK_NULL_HANDLE, depthHasStencil)) {
			DiscardImageStateFrame();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "후처리 장면 입력 패스 종료",
				"Vulkan 장면 입력 패스를 끝내지 못했습니다"));
		}
		return {};
	}

	GraphicsResult<void> VulkanPostProcess::Draw(
		const VulkanCommandBuffer& commandBuffers, const uint32_t imageIndex,
		const VkImage swapChainImage, const VkImageView swapChainImageView,
		const PostProcessFrameData& frameData) {
		const auto& routes = GetPassRoutes();
		if (imageIndex >= swapChainImageCount || imageIndex >= sceneTarget.GetImageCount()
			|| imageIndex >= frameDataBuffers.size()
			|| parameterDataBuffers.size() != swapChainImageCount
			|| !IsPassCountCompatible(pipelines.GetCount())
			|| !descriptors.IsCompatible(swapChainImageCount, routes.size())) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 효과 draw",
				"Vulkan 후처리 리소스 또는 실행 계획이 준비되지 않았습니다"));
		}
		const VkCommandBuffer commandBuffer = commandBuffers.TryGetCommandBuffer(imageIndex);
		if (commandBuffer == VK_NULL_HANDLE) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 효과 draw",
				"Vulkan command buffer를 사용할 수 없습니다"));
		}
		BeginImageStateFrame();
		if (!frameDataBuffers[imageIndex]->Write(&frameData, sizeof(frameData))) {
			DiscardImageStateFrame();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "후처리 frame data 기록",
				"Vulkan 후처리 frame buffer를 기록하지 못했습니다"));
		}
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
				DiscardImageStateFrame();
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ContractViolation, "후처리 pipeline binding",
					"Vulkan 후처리 pass에 대응하는 pipeline이 없습니다"));
			}
			const PostProcessPassRoute& route = routes[passIndex];
			const PostProcessParameterData& parameterData = GetParameterData(route);
			if (!parameterDataBuffers[imageIndex]->Write(
				&parameterData, sizeof(parameterData), passIndex * parameterDataStride)) {
				DiscardHistoryFrame();
				DiscardImageStateFrame();
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::CommandRecordingFailed, "후처리 parameter 기록",
					"Vulkan 후처리 pass parameter를 기록하지 못했습니다"));
			}
			if (!UpdateTextureDescriptorSet(imageIndex, passIndex)) {
				DiscardHistoryFrame();
				DiscardImageStateFrame();
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::CommandRecordingFailed, "후처리 descriptor 갱신",
					"Vulkan 후처리 texture descriptor를 갱신하지 못했습니다"));
			}
			VkImage outputImage = VK_NULL_HANDLE;
			VkImageView outputImageView = VK_NULL_HANDLE;
			VkExtent2D outputExtent{};
			bool outputInitialized = false;
			if (!ResolveOutputImage(route, imageIndex, swapChainImage, swapChainImageView,
				outputImage, outputImageView, outputExtent, outputInitialized)) {
				DiscardHistoryFrame();
				DiscardImageStateFrame();
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ContractViolation, "후처리 출력 target 조회",
					"Vulkan 후처리 pass의 출력 image를 찾지 못했습니다"));
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
		return {};
	}

	void VulkanPostProcess::CommitImageStateFrame() {
		velocityTarget.CommitInitializationFrame();
		for (auto& resource : resources)
			resource.CommitInitializationFrame();
	}

	void VulkanPostProcess::DiscardImageStateFrame() {
		velocityTarget.DiscardInitializationFrame();
		for (auto& resource : resources)
			resource.DiscardInitializationFrame();
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
