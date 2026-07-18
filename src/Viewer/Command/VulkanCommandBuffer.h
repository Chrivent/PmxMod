#pragma once

#include "Viewer/Buffer/VulkanBuffer.h"
#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/SwapChain/VulkanSwapChain.h"

#include <span>
#include <vector>

namespace Chrivent {
	// Vulkan 프레임 명령 버퍼의 할당, 기록과 해제를 관리한다.
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
		
		VkCommandBuffer TryGetCommandBuffer(const uint32_t imageIndex) const {
			return imageIndex < commandBuffers.size() ? commandBuffers[imageIndex] : VK_NULL_HANDLE;
		}
		
		// Synchronization2 배리어로 이미지 레이아웃과 접근 상태를 전환한다.
		static void TransitionImage(VkCommandBuffer commandBuffer, VkImage image,
			VkImageLayout oldLayout, VkImageLayout newLayout,
			VkPipelineStageFlags2 sourceStage, VkAccessFlags2 sourceAccess,
			VkPipelineStageFlags2 destinationStage, VkAccessFlags2 destinationAccess,
			VkImageAspectFlags aspectMask);
		// 현재 렌더링 영역에 맞는 동적 viewport와 scissor를 적용한다.
		static void ApplyViewportAndScissor(VkCommandBuffer commandBuffer, VkExtent2D extent);
		// 스왑체인 이미지 수에 맞춰 렌더링 명령 버퍼를 할당한다.
		bool Initialize(const VulkanDevice& sourceDevice, VkCommandPool sourceCommandPool, const VulkanSwapChain& sourceSwapChain);
		// 지정한 이미지 attachment로 dynamic rendering을 시작하고 파이프라인을 바인딩한다.
		bool BeginRecord(uint32_t imageIndex, VkImage colorImage, VkImageView colorImageView,
			VkImage resolveImage, VkImageView resolveImageView, VkImage depthImage,
			VkImageView depthImageView, bool depthHasStencil, VkSampleCountFlagBits sampleCount,
			VkPipeline pipeline, VkExtent2D extent, const float clearColor[4]);
		// 현재 command buffer에 graphics pipeline을 바인딩한다.
		void BindPipeline(uint32_t imageIndex, VkPipeline pipeline) const;
		// 현재 command buffer에 모델 vertex/index buffer를 바인딩하고 draw indexed 명령을 기록한다.
		void DrawIndexed(uint32_t imageIndex, const VulkanBuffer& vertexBuffer, const VulkanBuffer& indexBuffer, VkIndexType indexType, uint32_t firstIndex, uint32_t indexCount);
		// 현재 command buffer에 graphics descriptor set들을 바인딩한다.
		void BindDescriptorSets(uint32_t imageIndex, VkPipelineLayout pipelineLayout, uint32_t firstSet,
			std::span<const VkDescriptorSet> descriptorSets, std::span<const uint32_t> dynamicOffsets = {}) const;
		// 장면 렌더링을 끝내고 후처리용 단일 샘플 geometry 렌더링을 시작한다.
		bool BeginPostProcessSceneInputPass(uint32_t imageIndex, VkImage sceneImage, VkImage depthImage,
			VkImageView depthImageView, VkImage velocityImage, VkImageView velocityImageView,
			bool velocityInitialized, bool depthHasStencil, VkPipeline pipeline, VkExtent2D extent) const;
		// 후처리용 geometry 렌더링을 끝내고 depth/속도를 shader read 상태로 전환한다.
		bool EndPostProcessSceneInputPass(uint32_t imageIndex, VkImage depthImage,
			VkImage velocityImage, bool depthHasStencil) const;
		// dynamic rendering을 끝내고 출력 이미지를 present 상태로 전환한다.
		bool EndRecord(uint32_t imageIndex, VkImage outputImage) const;
		// 할당한 명령 버퍼를 해제한다.
		void Reset();
	};
}
