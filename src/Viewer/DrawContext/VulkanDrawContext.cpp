#include "Viewer/DrawContext/VulkanDrawContext.h"

#include "Viewer/Buffer/VulkanBuffer.h"
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
		const VkIndexType indexType, const size_t firstIndex, const size_t indexCount) {
		if (!frameReady || vertexBuffer.buffer == VK_NULL_HANDLE || indexBuffer.buffer == VK_NULL_HANDLE)
			return false;
		if (firstIndex > std::numeric_limits<uint32_t>::max()
			|| indexCount > std::numeric_limits<uint32_t>::max())
			return false;
		auto& commandBuffer = commandContext.GetCommandBuffer();
		if (bindStateCache.vertexBuffer != vertexBuffer.buffer) {
			if (!commandBuffer.BindVertexBuffer(currentImageIndex, vertexBuffer.buffer))
				return false;
			bindStateCache.vertexBuffer = vertexBuffer.buffer;
		}
		if (bindStateCache.indexBuffer != indexBuffer.buffer || bindStateCache.indexType != indexType) {
			if (!commandBuffer.BindIndexBuffer(currentImageIndex, indexBuffer.buffer, indexType))
				return false;
			bindStateCache.indexBuffer = indexBuffer.buffer;
			bindStateCache.indexType = indexType;
		}
		return commandBuffer.DrawIndexed(currentImageIndex,
			static_cast<uint32_t>(firstIndex), static_cast<uint32_t>(indexCount));
	}

	bool VulkanDrawContext::BindModelPipeline(const bool bothFace) {
		if (!frameReady)
			return false;
		const VkPipeline targetPipeline = pipeline.ResolveModelPipeline(bothFace);
		if (bindStateCache.pipeline == targetPipeline)
			return true;
		if (!commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline))
			return false;
		bindStateCache.pipeline = targetPipeline;
		return true;
	}

	bool VulkanDrawContext::BindSceneDepthPipeline(const bool bothFace) {
		if (!frameReady)
			return false;
		const VkPipeline targetPipeline = pipeline.ResolveSceneDepthPipeline(bothFace);
		if (bindStateCache.pipeline == targetPipeline)
			return true;
		if (!commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline))
			return false;
		bindStateCache.pipeline = targetPipeline;
		return true;
	}

	bool VulkanDrawContext::BindSceneVelocityPipeline(const bool bothFace) {
		if (!frameReady)
			return false;
		const VkPipeline targetPipeline = pipeline.ResolveSceneVelocityPipeline(bothFace);
		if (bindStateCache.pipeline == targetPipeline)
			return true;
		if (!commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline))
			return false;
		bindStateCache.pipeline = targetPipeline;
		return true;
	}

	bool VulkanDrawContext::BindEdgePipeline() {
		if (!frameReady)
			return false;
		const VkPipeline targetPipeline = pipeline.GetEdgePipeline();
		if (bindStateCache.pipeline == targetPipeline)
			return true;
		if (!commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline))
			return false;
		bindStateCache.pipeline = targetPipeline;
		return true;
	}

	bool VulkanDrawContext::BindGroundShadowPipeline() {
		if (!frameReady)
			return false;
		const VkPipeline targetPipeline = pipeline.GetGroundShadowPipeline();
		if (bindStateCache.pipeline == targetPipeline)
			return true;
		if (!commandContext.GetCommandBuffer().BindPipeline(currentImageIndex, targetPipeline))
			return false;
		bindStateCache.pipeline = targetPipeline;
		return true;
	}

	bool VulkanDrawContext::BindModelDescriptorSets(const VulkanDescriptorSet& descriptorSet,
		const uint32_t dynamicOffset) {
		if (!frameReady)
			return false;
		if (bindStateCache.vertexDescriptorSet == descriptorSet.GetVertexDescriptorSet() &&
			bindStateCache.vertexDynamicOffset == dynamicOffset)
			return true;
		const VkDescriptorSet vertexDescriptorSet = descriptorSet.GetVertexDescriptorSet();
		if (!commandContext.GetCommandBuffer().BindDescriptorSets(currentImageIndex,
			pipeline.GetPipelineLayout(), 0, { &vertexDescriptorSet, 1 }, { &dynamicOffset, 1 }))
			return false;
		bindStateCache.vertexDescriptorSet = vertexDescriptorSet;
		bindStateCache.vertexDynamicOffset = dynamicOffset;
		return true;
	}

	bool VulkanDrawContext::BindPixelDescriptorSet(const VkDescriptorSet descriptorSet,
		const uint32_t dynamicOffset) {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE)
			return false;
		if (bindStateCache.pixelDescriptorSet == descriptorSet &&
			bindStateCache.pixelDynamicOffset == dynamicOffset)
			return true;
		if (!commandContext.GetCommandBuffer().BindDescriptorSets(currentImageIndex,
			pipeline.GetPipelineLayout(), 1, { &descriptorSet, 1 }, { &dynamicOffset, 1 }))
			return false;
		bindStateCache.pixelDescriptorSet = descriptorSet;
		bindStateCache.pixelDynamicOffset = dynamicOffset;
		return true;
	}

	bool VulkanDrawContext::BindTextureDescriptorSet(const VkDescriptorSet descriptorSet) {
		if (!frameReady || descriptorSet == VK_NULL_HANDLE ||
			bindStateCache.textureDescriptorSet == descriptorSet)
			return frameReady && descriptorSet != VK_NULL_HANDLE;
		if (!commandContext.GetCommandBuffer().BindDescriptorSets(
			currentImageIndex, pipeline.GetPipelineLayout(), 2, { &descriptorSet, 1 }))
			return false;
		bindStateCache.textureDescriptorSet = descriptorSet;
		return true;
	}
}
