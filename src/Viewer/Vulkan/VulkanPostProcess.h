#pragma once

#include "Viewer/PostProcess.h"
#include "Viewer/Vulkan/Helper/VulkanBuffer.h"
#include "Viewer/Vulkan/Helper/VulkanDevice.h"
#include "Viewer/Vulkan/Helper/VulkanSwapChain.h"

#include <memory>
#include <vector>

namespace Chrivent {
	class VulkanCommandBuffer;
	struct PostProcessFrameData;

	class VulkanPostProcess : public PostProcess {
		VkDevice device = VK_NULL_HANDLE;
		std::vector<VkImage> targetImages;
		std::vector<VkDeviceMemory> targetImageMemories;
		std::vector<VkImageView> targetImageViews;
		std::vector<VkImage> depthImages;
		std::vector<VkDeviceMemory> depthImageMemories;
		std::vector<VkImageView> depthImageViews;
		std::vector<VkImage> focusHistoryImages;
		std::vector<VkDeviceMemory> focusHistoryImageMemories;
		std::vector<VkImageView> focusHistoryImageViews;
		std::vector<VkDescriptorSet> descriptorSets;
		std::vector<VkDescriptorSet> focusHistoryDescriptorSets;
		std::vector<VkDescriptorSet> frameDataDescriptorSets;
		std::vector<std::unique_ptr<VulkanBuffer>> frameDataBuffers;
		VkDescriptorSetLayout descriptorSetLayouts[3]{};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline focusHistoryPipeline = VK_NULL_HANDLE;
		std::vector<VkPipeline> pipelines;
		VkSampler sampler = VK_NULL_HANDLE;
		std::vector<size_t> focusHistoryIndices;
		std::vector<bool> focusHistoryInitialized;
		size_t swapChainImageCount = 0;

		// 스왑체인 이미지마다 장면 resolve와 sampling에 사용할 이미지를 생성한다.
		bool CreateTargetImages(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 스왑체인 이미지마다 후처리용 단일 샘플 depth 이미지를 생성한다.
		bool CreateDepthImages(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain, VkFormat depthFormat);
		// 스왑체인 이미지마다 DOF 초점 히스토리 ping-pong 이미지를 생성한다.
		bool CreateFocusHistoryImages(const VulkanDevice& sourceDevice);
		// 스왑체인 이미지마다 후처리 프레임 상수 버퍼를 생성한다.
		bool CreateFrameDataBuffers(const VulkanDevice& sourceDevice);
		// 장면 입력 texture/sampler용 descriptor 리소스를 생성한다.
		bool CreateDescriptors(const VulkanSwapChain&);
		// HLSL 후처리 pass 하나를 지정한 출력 크기와 형식의 pipeline으로 만든다.
		bool CreateGraphicsPipeline(const VulkanDevice& sourceDevice, const EffectPassDefinition& pass,
			VkExtent2D extent, VkFormat format, VkPipeline& pipeline) const;
		// 선택된 HLSL 후처리 실행 계획으로 풀스크린 graphics pipeline들을 생성한다.
		bool CreatePipelines(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// target 종류와 swapchain 이미지 인덱스로 평탄화한 리소스 인덱스를 계산한다.
		size_t ResolveTargetIndex(size_t targetIndex, uint32_t imageIndex) const;
		// focus history 종류와 swapchain 이미지 인덱스로 평탄화한 리소스 인덱스를 계산한다.
		size_t ResolveFocusHistoryIndex(size_t historyIndex, uint32_t imageIndex) const;
		// source target, swapchain 이미지, focus history 인덱스로 descriptor set 인덱스를 계산한다.
		size_t ResolveDescriptorIndex(size_t targetIndex, uint32_t imageIndex, size_t historyIndex) const;
		// 현재 swapchain 이미지의 초점 히스토리 ping-pong 인덱스를 다음 프레임용으로 갱신한다.
		void AdvanceFocusHistory(uint32_t imageIndex);

	public:
		~VulkanPostProcess() override;

		// 현재 스왑체인 이미지에 대응하는 장면 색상 resolve 이미지를 반환한다.
		VkImage ResolveSceneImage(uint32_t imageIndex) const {
			return imageIndex < swapChainImageCount ? targetImages[ResolveTargetIndex(0, imageIndex)] : VK_NULL_HANDLE;
		}
		// 현재 스왑체인 이미지에 대응하는 장면 색상 resolve 이미지 뷰를 반환한다.
		VkImageView ResolveSceneImageView(uint32_t imageIndex) const {
			return imageIndex < swapChainImageCount ? targetImageViews[ResolveTargetIndex(0, imageIndex)] : VK_NULL_HANDLE;
		}
		// 현재 스왑체인과 선택된 효과 목록에 맞는 Vulkan 후처리 리소스를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			VkFormat depthFormat);
		// Vulkan 포스트 프로세스용 단일 샘플 depth-only pass를 시작한다.
		bool BeginDepthPass(VulkanCommandBuffer& commandBuffers, uint32_t imageIndex,
			VkPipeline depthPipeline, VkExtent2D extent) const;
		// Vulkan 포스트 프로세스용 단일 샘플 depth-only pass를 종료한다.
		bool EndDepthPass(VulkanCommandBuffer& commandBuffers, uint32_t imageIndex) const;
		// 장면 렌더링을 끝내고 소유한 후처리 리소스로 최종 명령을 기록한다.
		bool EndRecord(VulkanCommandBuffer& commandBuffers, uint32_t imageIndex, VkImage swapChainImage,
			VkImageView swapChainImageView, VkExtent2D extent, const PostProcessFrameData& frameData,
			bool sceneRenderingEnded = false);
		// 다음 후처리 프레임에서 Vulkan 초점 히스토리를 0으로 초기화한다.
		void ResetHistory() override;
		// 생성한 Vulkan 후처리 리소스를 해제한다.
		void Reset() override;
	};
}
