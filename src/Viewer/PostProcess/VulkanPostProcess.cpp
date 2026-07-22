#include "Viewer/PostProcess/VulkanPostProcess.h"

#include "Viewer/PostProcess/PostProcessFrameData.h"
#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Buffer/BufferSize.h"
#include "Viewer/Command/VulkanCommandContext.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace Chrivent {
	VulkanPostProcess::~VulkanPostProcess() {
		VulkanPostProcess::ResetResources();
	}

	GraphicsError::Result<void> VulkanPostProcess::CreateSceneImages(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain) {
		swapChainImageCount = sourceSwapChain.GetImageCount();
		targetExtent = sourceSwapChain.GetExtent();
		swapChainFormat = sourceSwapChain.GetImageFormat();
		return sceneTarget.Initialize(sourceDevice, swapChainImageCount, targetExtent, swapChainFormat,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
	}

	GraphicsError::Result<void> VulkanPostProcess::CreateVelocityImages(const VulkanDevice& sourceDevice) {
		return velocityTarget.Initialize(sourceDevice, swapChainImageCount, targetExtent,
			VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, true);
	}

	VkFormat VulkanPostProcess::ResolveResourceFormat(const ResourcePlan& resource) {
		if (resource.format == EffectTextureFormat::Rgba8Unorm)
			return VK_FORMAT_R8G8B8A8_UNORM;
		return resource.format == EffectTextureFormat::Rgba16Float
			? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
	}

	VkExtent2D VulkanPostProcess::ResolveResourceExtent(const ResourcePlan& resource) const {
		return {
			static_cast<uint32_t>(PostProcess::ResolveResourceExtent(
				static_cast<int>(targetExtent.width), resource, true)),
			static_cast<uint32_t>(PostProcess::ResolveResourceExtent(
				static_cast<int>(targetExtent.height), resource, false))
		};
	}

	GraphicsError::Result<void> VulkanPostProcess::CreateEffectResources(const VulkanDevice& sourceDevice) {
		const auto plans = GetResourcePlans();
		resources.resize(plans.size());
		for (size_t resourceIndex = 0; resourceIndex < plans.size(); resourceIndex++) {
			const ResourcePlan& plan = plans[resourceIndex];
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

	GraphicsError::Result<void> VulkanPostProcess::CreateFrameDataBuffers(const VulkanDevice& sourceDevice) {
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

	GraphicsError::Result<void> VulkanPostProcess::CreateParameterDataBuffers(const VulkanDevice& sourceDevice) {
		parameterDataBuffers.clear();
		const size_t passCount = GetPassRoutes().size();
		const VkDeviceSize nativeAlignment = std::max<VkDeviceSize>(
			1, sourceDevice.GetUniformBufferAlignment());
		if (nativeAlignment > std::numeric_limits<size_t>::max()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "후처리 parameter buffer 크기 계산",
				"Vulkan uniform buffer 정렬이 프로그램 크기 범위를 벗어났습니다"));
		}
		size_t stride = 0;
		size_t bufferSize = 0;
		if (passCount == 0
			|| !BufferSize::TryAlignUp(
				sizeof(ParameterData), nativeAlignment, stride)
			|| !BufferSize::TryMultiply(passCount, stride, bufferSize)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "후처리 parameter buffer 크기 계산",
				"Vulkan 후처리 패스 수가 parameter buffer 크기 한도를 넘습니다"));
		}
		parameterDataStride = stride;
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
		const PassInputRoute& input, const uint32_t imageIndex) const {
		if (input.kind == InputKind::SceneColor)
			return sceneTarget.TryGetImageView(imageIndex);
		if (input.kind == InputKind::SceneDepth)
			return depthTarget.TryGetImageView(imageIndex);
		if (input.kind == InputKind::SceneVelocity)
			return velocityTarget.TryGetImageView(imageIndex);
		if (input.resourceIndex >= resources.size())
			return VK_NULL_HANDLE;
		const VulkanPostProcessTarget& resource = resources[input.resourceIndex];
		const size_t index = ResolveResourceReadIndex(input.resourceIndex, imageIndex);
		return resource.TryGetImageView(index);
	}

	bool VulkanPostProcess::UpdateTextureDescriptorSet(
		const uint32_t imageIndex, const size_t passIndex) {
		const auto routes = GetPassRoutes();
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

	GraphicsError::Result<void> VulkanPostProcess::CreatePipelines(const VulkanDevice& sourceDevice) {
		const auto passes = GetShaderPrograms();
		const auto routes = GetPassRoutes();
		std::vector<VkFormat> targetFormats;
		targetFormats.reserve(passes.size());
		for (size_t index = 0; index < passes.size(); index++) {
			VkFormat format = swapChainFormat;
			if (routes[index].outputKind == OutputKind::Resource) {
				const ResourcePlan& resource = GetResourcePlans()[routes[index].outputResourceIndex];
				format = ResolveResourceFormat(resource);
			}
			targetFormats.push_back(format);
		}
		if (pipelines.IsCompatible(targetFormats))
			return {};
		return pipelines.Initialize(
			sourceDevice, descriptors.GetPipelineLayout(), passes, targetFormats);
	}

	bool VulkanPostProcess::ResolveOutputImage(const PassRoute& route, const uint32_t imageIndex,
		const VkImage swapChainImage, const VkImageView swapChainImageView, VkImage& image,
		VkImageView& imageView, VkExtent2D& extent, bool& initialized) {
		if (route.outputKind == OutputKind::Present) {
			image = swapChainImage;
			imageView = swapChainImageView;
			extent = targetExtent;
			initialized = false;
			return image != VK_NULL_HANDLE && imageView != VK_NULL_HANDLE;
		}
		if (route.outputResourceIndex >= resources.size())
			return false;
		VulkanPostProcessTarget& resource = resources[route.outputResourceIndex];
		const ResourcePlan& plan = GetResourcePlans()[route.outputResourceIndex];
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

	void VulkanPostProcess::SwapResources(VulkanPostProcess& other) noexcept {
		std::swap(device, other.device);
		std::swap(targetExtent, other.targetExtent);
		std::swap(swapChainFormat, other.swapChainFormat);
		std::swap(sceneTarget, other.sceneTarget);
		std::swap(depthTarget, other.depthTarget);
		std::swap(velocityTarget, other.velocityTarget);
		resources.swap(other.resources);
		frameDataBuffers.swap(other.frameDataBuffers);
		parameterDataBuffers.swap(other.parameterDataBuffers);
		descriptors.Swap(other.descriptors);
		pipelines.Swap(other.pipelines);
		std::swap(swapChainImageCount, other.swapChainImageCount);
		std::swap(parameterDataStride, other.parameterDataStride);
	}

	GraphicsError::Result<void> VulkanPostProcess::InitializeTargets(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat) {
		ResetTargets();
		device = sourceDevice.GetDevice();
		auto result = CreateSceneImages(sourceDevice, sourceSwapChain);
		if (result && (RequiresDepth() || RequiresVelocity())) {
			result = depthTarget.Initialize(
				sourceDevice, swapChainImageCount, sourceSwapChain.GetExtent(), depthFormat,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				false, VK_IMAGE_ASPECT_DEPTH_BIT);
		}
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
				sizeof(ParameterData), parameterDataStride);
		}
		if (result)
			result = CreatePipelines(sourceDevice);
		if (result)
			return {};
		const GraphicsError error = result.error();
		ResetTargets();
		return std::unexpected(error);
	}

	GraphicsError::Result<void> VulkanPostProcess::Configure(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat, PreparedPostProcessEffects preparedEffects) {
		VulkanPostProcess candidate;
		candidate.AdoptPreparedEffects(std::move(preparedEffects));
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
	
	GraphicsError::Result<void> VulkanPostProcess::BeginSceneInputPass(const VulkanCommandContext& commandContext,
		const uint32_t imageIndex, const VkExtent2D extent) {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 시작",
				"Vulkan 후처리 장면 입력 target 또는 image index가 올바르지 않습니다"));
		}
		BeginImageStateFrame();
		constexpr bool depthHasStencil = false;
		const bool began = commandContext.BeginPostProcessSceneInputPass(imageIndex,
			sceneTarget.TryGetImage(imageIndex), depthTarget.TryGetImage(imageIndex),
			depthTarget.TryGetImageView(imageIndex),
			RequiresVelocity() ? velocityTarget.TryGetImage(imageIndex) : VK_NULL_HANDLE,
			RequiresVelocity() ? velocityTarget.TryGetImageView(imageIndex) : VK_NULL_HANDLE,
			RequiresVelocity() && velocityTarget.IsInitialized(imageIndex), depthHasStencil, extent);
		if (!began) {
			DiscardImageStateFrame();
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "후처리 장면 입력 패스 시작",
				"Vulkan 장면 입력 패스를 시작하지 못했습니다"));
		}
		if (RequiresVelocity())
			velocityTarget.MarkInitialized(imageIndex);
		return {};
	}

	GraphicsError::Result<void> VulkanPostProcess::EndSceneInputPass(
		const VulkanCommandContext& commandContext, const uint32_t imageIndex) {
		if ((!RequiresDepth() && !RequiresVelocity()) || imageIndex >= swapChainImageCount) {
			DiscardImageStateFrame();
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 장면 입력 패스 종료",
				"Vulkan 후처리 장면 입력 target 또는 image index가 올바르지 않습니다"));
		}
		constexpr bool depthHasStencil = false;
		if (!commandContext.EndPostProcessSceneInputPass(
			imageIndex, depthTarget.TryGetImage(imageIndex),
			RequiresVelocity() ? velocityTarget.TryGetImage(imageIndex) : VK_NULL_HANDLE, depthHasStencil)) {
			DiscardImageStateFrame();
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "후처리 장면 입력 패스 종료",
				"Vulkan 장면 입력 패스를 끝내지 못했습니다"));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanPostProcess::Draw(
		const VulkanCommandContext& commandContext, const uint32_t imageIndex,
		const VkImage swapChainImage, const VkImageView swapChainImageView,
		const PostProcessFrameData& frameData) {
		const auto routes = GetPassRoutes();
		if (imageIndex >= swapChainImageCount || imageIndex >= sceneTarget.GetImageCount()
			|| imageIndex >= frameDataBuffers.size()
			|| parameterDataBuffers.size() != swapChainImageCount
			|| !IsPassCountCompatible(pipelines.GetCount())
			|| !descriptors.IsCompatible(swapChainImageCount, routes.size())) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 효과 draw",
				"Vulkan 후처리 리소스 또는 실행 계획이 준비되지 않았습니다"));
		}
		const VkCommandBuffer commandBuffer = commandContext.TryGetCommandBuffer(imageIndex);
		if (commandBuffer == VK_NULL_HANDLE) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 효과 draw",
				"Vulkan command buffer를 사용할 수 없습니다"));
		}
		BeginImageStateFrame();
		if (!frameDataBuffers[imageIndex]->Write(&frameData, sizeof(frameData))) {
			DiscardImageStateFrame();
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "후처리 frame data 기록",
				"Vulkan 후처리 frame buffer를 기록하지 못했습니다"));
		}
		const VkPipelineLayout pipelineLayout = descriptors.GetPipelineLayout();
		const VkDescriptorSet frameDataDescriptorSet =
			descriptors.TryGetFrameDataDescriptorSet(imageIndex);
		if (pipelineLayout == VK_NULL_HANDLE || frameDataDescriptorSet == VK_NULL_HANDLE) {
			DiscardImageStateFrame();
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "후처리 frame descriptor binding",
				"Vulkan 후처리 pipeline layout 또는 frame descriptor set을 사용할 수 없습니다"));
		}
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
		const auto resourcePlans = GetResourcePlans();
		for (size_t index = 0; index < resources.size() && index < resourcePlans.size(); index++) {
			const VulkanPostProcessTarget& resource = resources[index];
			if (!NeedsHistoryInitialization(index))
				continue;
			for (const VkImage image : resource.GetImages()) {
				VulkanCommandContext::TransitionImage(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE,
					VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
					VK_IMAGE_ASPECT_COLOR_BIT);
				vkCmdClearColorImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					&clearColor, 1, &colorRange);
				VulkanCommandContext::TransitionImage(commandBuffer, image,
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
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::ContractViolation, "후처리 pipeline binding",
					"Vulkan 후처리 pass에 대응하는 pipeline이 없습니다"));
			}
			const PassRoute& route = routes[passIndex];
			const ParameterData& parameterData = GetParameterData(route);
			if (!parameterDataBuffers[imageIndex]->Write(
				&parameterData, sizeof(parameterData), passIndex * parameterDataStride)) {
				DiscardHistoryFrame();
				DiscardImageStateFrame();
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::CommandRecordingFailed, "후처리 parameter 기록",
					"Vulkan 후처리 pass parameter를 기록하지 못했습니다"));
			}
			if (!UpdateTextureDescriptorSet(imageIndex, passIndex)) {
				DiscardHistoryFrame();
				DiscardImageStateFrame();
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
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
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::ContractViolation, "후처리 출력 target 조회",
					"Vulkan 후처리 pass의 출력 image를 찾지 못했습니다"));
			}
			VulkanCommandContext::TransitionImage(commandBuffer, outputImage,
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
			const VkDescriptorSet descriptorSet =
				descriptors.TryGetTextureDescriptorSet(imageIndex, passIndex);
			const VkDescriptorSet parameterSet =
				descriptors.TryGetParameterDataDescriptorSet(imageIndex, passIndex);
			if (descriptorSet == VK_NULL_HANDLE || parameterSet == VK_NULL_HANDLE) {
				DiscardHistoryFrame();
				DiscardImageStateFrame();
				return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
					GraphicsErrorCode::InvalidState, "후처리 pass descriptor binding",
					"Vulkan 후처리 texture 또는 parameter descriptor set을 사용할 수 없습니다"));
			}
			vkCmdBeginRendering(commandBuffer, &renderingInfo);
			VulkanCommandContext::ApplyViewportAndScissor(commandBuffer, outputExtent);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.TryGetPipeline(passIndex));
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 1, 1, &parameterSet, 0, nullptr);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout, 2, 1, &descriptorSet, 0, nullptr);
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
			vkCmdEndRendering(commandBuffer);
			if (route.outputKind == OutputKind::Resource) {
				VulkanCommandContext::TransitionImage(commandBuffer, outputImage,
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

	void VulkanPostProcess::ResetTargets() {
		descriptors.Reset();
		sceneTarget.Reset();
		depthTarget.Reset();
		velocityTarget.Reset();
		resources.clear();
		frameDataBuffers.clear();
		parameterDataBuffers.clear();
		swapChainImageCount = 0;
		parameterDataStride = 0;
		targetExtent = {};
		swapChainFormat = VK_FORMAT_UNDEFINED;
		device = VK_NULL_HANDLE;
	}

	void VulkanPostProcess::ResetResources() {
		pipelines.Reset();
		ResetTargets();
	}
}
