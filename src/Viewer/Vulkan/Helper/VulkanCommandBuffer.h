#pragma once

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

#include <vector>

namespace Chrivent {
	class VulkanCommandBuffer {
		std::vector<VkCommandBuffer> commandBuffers;
		std::vector<VkBuffer> boundVertexBuffers;
		std::vector<VkBuffer> boundIndexBuffers;
		std::vector<VkIndexType> boundIndexTypes;
		VkDevice device = VK_NULL_HANDLE;
		VkCommandPool commandPool = VK_NULL_HANDLE;

	public:
		VulkanCommandBuffer() = default;
		~VulkanCommandBuffer();

		VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
		VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;
		VulkanCommandBuffer(VulkanCommandBuffer&&) = delete;
		VulkanCommandBuffer& operator=(VulkanCommandBuffer&&) = delete;
		
		VkCommandBuffer GetCommandBuffer(const uint32_t imageIndex) const { return commandBuffers[imageIndex]; }
		const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return commandBuffers; }

		// 스왑체인 이미지 수에 맞춰 렌더링 명령 버퍼를 할당한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, VkCommandPool sourceCommandPool, const VulkanSwapChainInfo& swapChainInfo);
		// 지정한 스왑체인 이미지에 대한 렌더 패스를 시작하고 파이프라인을 바인딩한다.
		bool BeginRecord(uint32_t imageIndex, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkPipeline pipeline, VkExtent2D extent, const float clearColor[4]);
		// 현재 command buffer에 graphics pipeline을 바인딩한다.
		void BindPipeline(uint32_t imageIndex, VkPipeline pipeline) const;
		// 현재 command buffer에 모델 vertex/index buffer를 바인딩하고 draw indexed 명령을 기록한다.
		void DrawIndexed(uint32_t imageIndex, const VulkanBufferInfo& vertexBuffer, const VulkanBufferInfo& indexBuffer, VkIndexType indexType, uint32_t firstIndex, uint32_t indexCount);
		// 현재 command buffer에 graphics descriptor set들을 바인딩한다.
		void BindDescriptorSets(
			uint32_t imageIndex,
			VkPipelineLayout pipelineLayout,
			uint32_t firstSet,
			const VkDescriptorSet* descriptorSets,
			uint32_t descriptorSetCount,
			const uint32_t* dynamicOffsets = nullptr,
			uint32_t dynamicOffsetCount = 0) const;
		// 지정한 스왑체인 이미지에 대한 렌더 패스와 command buffer 기록을 종료한다.
		bool EndRecord(uint32_t imageIndex) const;
		// 할당한 명령 버퍼를 해제한다.
		void Destroy();
	};
}
