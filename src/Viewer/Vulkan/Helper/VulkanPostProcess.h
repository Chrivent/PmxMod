#pragma once

#include "Viewer/Shader/ShaderPackage.h"
#include "Viewer/Vulkan/Helper/VulkanDevice.h"
#include "Viewer/Vulkan/Helper/VulkanSwapChain.h"

#include <vector>

namespace Chrivent {
	class VulkanPostProcess {
		VkDevice device = VK_NULL_HANDLE;
		std::vector<VkImage> sceneImages;
		std::vector<VkDeviceMemory> sceneImageMemories;
		std::vector<VkImageView> sceneImageViews;
		std::vector<VkDescriptorSet> descriptorSets;
		VkDescriptorSetLayout descriptorSetLayouts[3]{};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;

		// 스왑체인 이미지마다 장면 resolve와 sampling에 사용할 이미지를 생성한다.
		bool CreateSceneImages(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 장면 입력 texture/sampler용 descriptor 리소스를 생성한다.
		bool CreateDescriptors(const VulkanSwapChain& sourceSwapChain);
		// 선택된 HLSL 후처리 효과로 풀스크린 graphics pipeline을 생성한다.
		bool CreatePipeline(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			const EffectDefinition& effect);

	public:
		VulkanPostProcess() = default;
		~VulkanPostProcess();

		VulkanPostProcess(const VulkanPostProcess&) = delete;
		VulkanPostProcess& operator=(const VulkanPostProcess&) = delete;
		VulkanPostProcess(VulkanPostProcess&&) = delete;
		VulkanPostProcess& operator=(VulkanPostProcess&&) = delete;

		const std::vector<VkImageView>& GetSceneImageViews() const { return sceneImageViews; }
		VkImage GetSceneImage(uint32_t imageIndex) const { return sceneImages[imageIndex]; }
		VkPipeline GetPipeline() const { return pipeline; }
		VkPipelineLayout GetPipelineLayout() const { return pipelineLayout; }
		VkDescriptorSet GetDescriptorSet(uint32_t imageIndex) const { return descriptorSets[imageIndex]; }

		// 현재 스왑체인과 선택된 효과에 맞는 Vulkan 후처리 리소스를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			const EffectDefinition& effect);
		// 생성한 Vulkan 후처리 리소스를 해제한다.
		void Reset();
	};
}
