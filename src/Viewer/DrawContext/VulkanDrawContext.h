#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <limits>

namespace Chrivent {
	class VulkanBuffer;
	class VulkanCommandContext;
	class VulkanDescriptorSet;
	class VulkanPipeline;

	// Vulkan Drawer에 현재 command buffer와 장면 pipeline/descriptor 바인딩을 제공한다.
	class VulkanDrawContext {
		// 중복 Vulkan pipeline 및 descriptor 바인딩을 생략하기 위한 현재 상태를 기록한다.
		struct BindStateCache {
			VkPipeline pipeline = VK_NULL_HANDLE;
			VkDescriptorSet vertexDescriptorSet = VK_NULL_HANDLE;
			uint32_t vertexDynamicOffset = std::numeric_limits<uint32_t>::max();
			VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
			uint32_t pixelDynamicOffset = std::numeric_limits<uint32_t>::max();
			VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;
		};

		VulkanPipeline& pipeline;
		VulkanCommandContext& commandContext;
		uint32_t currentImageIndex = 0;
		bool frameReady = false;
		size_t frameIndex = 0;
		BindStateCache bindStateCache;

	public:
		VulkanDrawContext(VulkanPipeline& sourcePipeline, VulkanCommandContext& sourceCommandContext);

		size_t GetFrameIndex() const { return frameIndex; }
		uint32_t GetCurrentImageIndex() const { return currentImageIndex; }
		bool IsFrameReady() const { return frameReady; }

		// 새 프레임 상태를 저장하고 pipeline 및 descriptor 바인딩 캐시를 초기화한다.
		void BeginFrame(uint32_t sourceCurrentImageIndex, size_t sourceFrameIndex);
		// 현재 프레임의 Drawer 명령 기록을 막는다.
		void EndFrame();
		// 외부 패스가 먼저 바인딩한 pipeline을 현재 캐시에 반영한다.
		void SetPipelineState(VkPipeline sourcePipeline);
		// 장면 입력 패스 전환 뒤 descriptor 바인딩 캐시를 초기화한다.
		void ResetDescriptorBindings();
		// 현재 command buffer에 indexed draw 명령을 기록한다.
		bool DrawIndexed(const VulkanBuffer& vertexBuffer, const VulkanBuffer& indexBuffer,
			VkIndexType indexType, size_t firstIndex, size_t indexCount) const;
		// 재질 양면 여부에 맞는 model pipeline을 바인딩한다.
		void BindModelPipeline(bool bothFace);
		// 재질 양면 여부에 맞는 depth-only pipeline을 바인딩한다.
		void BindDepthOnlyPipeline(bool bothFace);
		// 재질 양면 여부에 맞는 scene velocity pipeline을 바인딩한다.
		void BindSceneVelocityPipeline(bool bothFace);
		// 엣지 pipeline을 바인딩한다.
		void BindEdgePipeline();
		// 지면 그림자 pipeline을 바인딩한다.
		void BindGroundShadowPipeline();
		// 모델 공통 vertex descriptor set을 바인딩한다.
		void BindModelDescriptorSets(const VulkanDescriptorSet& descriptorSet, uint32_t dynamicOffset);
		// 재질 pixel descriptor set을 바인딩한다.
		void BindPixelDescriptorSet(VkDescriptorSet descriptorSet, uint32_t dynamicOffset);
		// 재질 texture descriptor set을 바인딩한다.
		void BindTextureDescriptorSet(VkDescriptorSet descriptorSet);
	};
}
