#pragma once

#include "Viewer/PostProcess.h"
#include "Viewer/Vulkan/Helper/VulkanDevice.h"
#include "Viewer/Vulkan/Helper/VulkanSwapChain.h"

#include <vector>

namespace Chrivent {
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
		// 장면 입력 texture/sampler용 descriptor 리소스를 생성한다.
		bool CreateDescriptors(const VulkanSwapChain&);
		// 선택된 HLSL 후처리 효과 목록으로 풀스크린 graphics pipeline들을 생성한다.
		bool CreatePipeline(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// target 종류와 swapchain 이미지 인덱스로 평탄화한 리소스 인덱스를 계산한다.
		size_t ResolveTargetIndex(size_t targetIndex, uint32_t imageIndex) const;
		// focus history 종류와 swapchain 이미지 인덱스로 평탄화한 리소스 인덱스를 계산한다.
		size_t ResolveFocusHistoryIndex(size_t historyIndex, uint32_t imageIndex) const;
		// source target, swapchain 이미지, focus history 인덱스로 descriptor set 인덱스를 계산한다.
		size_t ResolveDescriptorIndex(size_t targetIndex, uint32_t imageIndex, size_t historyIndex) const;

	public:
		VulkanPostProcess() = default;
		~VulkanPostProcess() override;

		VulkanPostProcess(const VulkanPostProcess&) = delete;
		VulkanPostProcess& operator=(const VulkanPostProcess&) = delete;
		VulkanPostProcess(VulkanPostProcess&&) = delete;
		VulkanPostProcess& operator=(VulkanPostProcess&&) = delete;

		VkImage GetTargetImage(const size_t targetIndex, const uint32_t imageIndex) const { return targetImages[ResolveTargetIndex(targetIndex, imageIndex)]; }
		VkImageView GetTargetImageView(const size_t targetIndex, const uint32_t imageIndex) const { return targetImageViews[ResolveTargetIndex(targetIndex, imageIndex)]; }
		VkImage GetDepthImage(const uint32_t imageIndex) const { return depthImages[imageIndex]; }
		VkImageView GetDepthImageView(const uint32_t imageIndex) const { return depthImageViews[imageIndex]; }
		VkImage GetFocusHistoryImage(const size_t historyIndex, const uint32_t imageIndex) const { return focusHistoryImages[ResolveFocusHistoryIndex(historyIndex, imageIndex)]; }
		VkImageView GetFocusHistoryImageView(const size_t historyIndex, const uint32_t imageIndex) const { return focusHistoryImageViews[ResolveFocusHistoryIndex(historyIndex, imageIndex)]; }
		const std::vector<VkPipeline>& GetPipelines() const { return pipelines; }
		VkPipeline GetFocusHistoryPipeline() const { return focusHistoryPipeline; }
		VkPipelineLayout GetPipelineLayout() const { return pipelineLayout; }
		VkDescriptorSet GetDescriptorSet(const size_t targetIndex, const uint32_t imageIndex, const size_t historyIndex) const { return descriptorSets[ResolveDescriptorIndex(targetIndex, imageIndex, historyIndex)]; }
		VkDescriptorSet GetFocusHistoryDescriptorSet(const uint32_t imageIndex, const size_t historyIndex) const { return focusHistoryDescriptorSets[ResolveFocusHistoryIndex(historyIndex, imageIndex)]; }
		size_t GetFocusHistoryReadIndex(const uint32_t imageIndex) const { return focusHistoryIndices[imageIndex]; }
		size_t GetFocusHistoryWriteIndex(const uint32_t imageIndex) const { return 1 - focusHistoryIndices[imageIndex]; }
		bool HasFocusHistoryEffect() const { return focusHistoryPipeline != VK_NULL_HANDLE; }
		bool IsFocusHistoryInitialized(const uint32_t imageIndex) const { return focusHistoryInitialized[imageIndex]; }
		static size_t GetTargetCount() { return targetCount; }
		
		// 현재 swapchain 이미지의 초점 히스토리 ping-pong 인덱스를 다음 프레임용으로 갱신한다.
		void AdvanceFocusHistory(uint32_t imageIndex);

		// 현재 스왑체인과 선택된 효과 목록에 맞는 Vulkan 후처리 리소스를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			VkFormat depthFormat);
		// 생성한 Vulkan 후처리 리소스를 해제한다.
		void Reset() override;
	};
}
