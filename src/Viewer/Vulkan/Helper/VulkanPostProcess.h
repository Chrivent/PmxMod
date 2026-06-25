#pragma once

#include "Viewer/Shader/ShaderPackage.h"
#include "Viewer/Vulkan/Helper/VulkanDevice.h"
#include "Viewer/Vulkan/Helper/VulkanSwapChain.h"

#include <vector>

namespace Chrivent {
	class VulkanPostProcess {
		VkDevice device = VK_NULL_HANDLE;
		static constexpr size_t kTargetCount = 3;
		std::vector<VkImage> targetImages;
		std::vector<VkDeviceMemory> targetImageMemories;
		std::vector<VkImageView> targetImageViews;
		std::vector<VkDescriptorSet> descriptorSets;
		VkDescriptorSetLayout descriptorSetLayouts[3]{};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		std::vector<VkPipeline> pipelines;
		VkSampler sampler = VK_NULL_HANDLE;
		size_t swapChainImageCount = 0;

		// 스왑체인 이미지마다 장면 resolve와 sampling에 사용할 이미지를 생성한다.
		bool CreateTargetImages(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 장면 입력 texture/sampler용 descriptor 리소스를 생성한다.
		bool CreateDescriptors(const VulkanSwapChain&);
		// 선택된 HLSL 후처리 효과 목록으로 풀스크린 graphics pipeline들을 생성한다.
		bool CreatePipeline(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			const std::vector<const EffectDefinition*>& effects);
		// target 종류와 swapchain 이미지 인덱스로 평탄화한 리소스 인덱스를 계산한다.
		size_t ResolveTargetIndex(size_t targetIndex, uint32_t imageIndex) const;

	public:
		VulkanPostProcess() = default;
		~VulkanPostProcess();

		VulkanPostProcess(const VulkanPostProcess&) = delete;
		VulkanPostProcess& operator=(const VulkanPostProcess&) = delete;
		VulkanPostProcess(VulkanPostProcess&&) = delete;
		VulkanPostProcess& operator=(VulkanPostProcess&&) = delete;

		VkImage GetTargetImage(size_t targetIndex, uint32_t imageIndex) const { return targetImages[ResolveTargetIndex(targetIndex, imageIndex)]; }
		VkImageView GetTargetImageView(size_t targetIndex, uint32_t imageIndex) const { return targetImageViews[ResolveTargetIndex(targetIndex, imageIndex)]; }
		const std::vector<VkPipeline>& GetPipelines() const { return pipelines; }
		VkPipelineLayout GetPipelineLayout() const { return pipelineLayout; }
		VkDescriptorSet GetDescriptorSet(const size_t targetIndex, const uint32_t imageIndex) const { return descriptorSets[ResolveTargetIndex(targetIndex, imageIndex)]; }
		static size_t GetTargetCount() { return kTargetCount; }

		// 현재 스왑체인과 선택된 효과에 맞는 Vulkan 후처리 리소스를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			const EffectDefinition& effect);
		// 현재 스왑체인과 선택된 효과 목록에 맞는 Vulkan 후처리 리소스를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			const std::vector<const EffectDefinition*>& effects);
		// 생성한 Vulkan 후처리 리소스를 해제한다.
		void Reset();
	};
}
