#include "Viewer/DrawContext/VulkanDrawContext.h"

#include "Viewer/Command/VulkanCommandContext.h"
#include "Viewer/Descriptor/VulkanDescriptorSet.h"
#include "Viewer/Pipeline/VulkanPipeline.h"

namespace Chrivent {
	VulkanDrawContext::VulkanDrawContext(VulkanPipeline& sourcePipeline,
		VulkanCommandContext& sourceCommandContext)
		: pipeline(sourcePipeline), commandContext(sourceCommandContext) {}

	void VulkanDrawContext::BeginFrame(const uint32_t sourceCurrentImageIndex, const size_t sourceFrameIndex) {
		currentImageIndex = sourceCurrentImageIndex;
		frameIndex = sourceFrameIndex;
		frameReady = true;
		bindStateCache = {};
	}

	void VulkanDrawContext::EndFrame() {
		frameReady = false;
	}

	void VulkanDrawContext::ResetDescriptorBindings() {
		bindStateCache.vertexDescriptorSet = VK_NULL_HANDLE;
		bindStateCache.vertexDynamicOffset = std::numeric_limits<uint32_t>::max();
		bindStateCache.pixelDescriptorSet = VK_NULL_HANDLE;
		bindStateCache.pixelDynamicOffset = std::numeric_limits<uint32_t>::max();
		bindStateCache.textureDescriptorSet = VK_NULL_HANDLE;
	}

	bool VulkanDrawContext::DrawIndexed(const VulkanBuffer& vertexBuffer, const VulkanBuffer& indexBuffer,
		const VkIndexType indexType, const size_t firstIndex, const size_t indexCount) const {
		if (!frameReady)
			return false;
		if (firstIndex > std::numeric_limits<uint32_t>::max()
			|| indexCount > std::numeric_limits<uint32_t>::max())
			return false;
		return commandContext.GetCommandBuffer().DrawIndexed(
			currentImageIndex, vertexBuffer, indexBuffer, indexType, firstIndex, indexCount);
	}

	void VulkanDrawContext::BindModelPipeline(const bool bothFace) {
		if (!frameReady)
			return;
		const VkPipeline targetPipeline = pipeline.ResolveModelPipeline(bothFace);
		if (bindStateCache.pipeline == targetPipeline)
			return;
		commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline);
		bindStateCache.pipeline = targetPipeline;
	}

	void VulkanDrawContext::BindDepthOnlyPipeline(const bool bothFace) {
		if (!frameReady)
			return;
		const VkPipeline targetPipeline = pipeline.ResolveSceneInputPipeline(false, bothFace);
		if (bindStateCache.pipeline == targetPipeline)
			return;
		commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline);
		bindStateCache.pipeline = targetPipeline;
	}

	void VulkanDrawContext::BindSceneVelocityPipeline(const bool bothFace) {
		if (!frameReady)
			return;
		const VkPipeline targetPipeline = pipeline.ResolveSceneInputPipeline(true, bothFace);
		if (bindStateCache.pipeline == targetPipeline)
			return;
		commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline);
		bindStateCache.pipeline = targetPipeline;
	}

	void VulkanDrawContext::BindEdgePipeline() {
		if (!frameReady)
			return;
		const VkPipeline targetPipeline = pipeline.GetEdgePipeline();
		if (bindStateCache.pipeline == targetPipeline)
			return;
		commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline);
		bindStateCache.pipeline = targetPipeline;
	}

	void VulkanDrawContext::BindGroundShadowPipeline() {
		if (!frameReady)
			return;
		const VkPipeline targetPipeline = pipeline.GetGroundShadowPipeline();
		if (bindStateCache.pipeline == targetPipeline)
			return;
		commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline);
		bindStateCache.pipeline = targetPipeline;
	}

	void VulkanDrawContext::BindModelDescriptorSets(const VulkanDescriptorSet& descriptorSet,
		const uint32_t dynamicOffset) {
		if (!frameReady)
			return;
		if (bindStateCache.vertexDescriptorSet == descriptorSet.GetVertexDescriptorSet() &&
			bindStateCache.vertexDynamicOffset == dynamicOffset)
			return;
		const VkDescriptorSet vertexDescriptorSet = descriptorSet.GetVertexDescriptorSet();
		commandContext.GetCommandBuffer().BindDescriptorSets(currentImageIndex, pipeline.GetPipelineLayout(), 0,
			{ &vertexDescriptorSet, 1 }, { &dynamicOffset, 1 });
		bindStateCache.vertexDescriptorSet = vertexDescriptorSet;
		bindStateCache.vertexDynamicOffset = dynamicOffset;
	}

	void VulkanDrawContext::BindPixelDescriptorSet(const VkDescriptorSet descriptorSet,
		const uint32_t dynamicOffset) {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE)
			return;
		if (bindStateCache.pixelDescriptorSet == descriptorSet &&
			bindStateCache.pixelDynamicOffset == dynamicOffset)
			return;
		commandContext.GetCommandBuffer().BindDescriptorSets(currentImageIndex, pipeline.GetPipelineLayout(), 1,
			{ &descriptorSet, 1 }, { &dynamicOffset, 1 });
		bindStateCache.pixelDescriptorSet = descriptorSet;
		bindStateCache.pixelDynamicOffset = dynamicOffset;
	}

	void VulkanDrawContext::BindTextureDescriptorSet(const VkDescriptorSet descriptorSet) {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE ||
			bindStateCache.textureDescriptorSet == descriptorSet)
			return;
		commandContext.GetCommandBuffer().BindDescriptorSets(currentImageIndex, pipeline.GetPipelineLayout(), 2,
			{ &descriptorSet, 1 });
		bindStateCache.textureDescriptorSet = descriptorSet;
	}
}
