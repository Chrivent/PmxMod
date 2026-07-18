#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/PostProcess/PostProcessRuntimeContract.h"

#include <span>
#include <vector>

namespace Chrivent {
	// Vulkan 후처리 패스별 동적 viewport 그래픽 파이프라인을 생성하고 소유한다.
	class VulkanPostProcessPipelines {
		VkDevice device = VK_NULL_HANDLE;
		std::vector<VkPipeline> pipelines;

		// HLSL 후처리 패스 하나를 지정한 출력 형식의 파이프라인으로 만든다.
		bool CreateGraphicsPipeline(const VulkanDevice& sourceDevice, VkPipelineLayout pipelineLayout,
			const EffectPassDefinition& pass, VkFormat targetFormat, VkPipeline& pipeline) const;

	public:
		VulkanPostProcessPipelines() = default;
		~VulkanPostProcessPipelines();
		
		VulkanPostProcessPipelines(const VulkanPostProcessPipelines&) = delete;
		VulkanPostProcessPipelines& operator=(const VulkanPostProcessPipelines&) = delete;

		VkPipeline TryGetPipeline(const size_t index) const {
			return index < pipelines.size() ? pipelines[index] : VK_NULL_HANDLE;
		}
		size_t GetCount() const { return pipelines.size(); }

		// 패스와 출력 정보 목록을 검증한 뒤 모든 Vulkan 후처리 파이프라인을 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, VkPipelineLayout pipelineLayout,
			std::span<const EffectPassDefinition> passes,
			std::span<const VkFormat> targetFormats);
		// 다른 파이프라인 묶음과 소유권을 교환한다.
		void Swap(VulkanPostProcessPipelines& other) noexcept;
		// 생성한 Vulkan 파이프라인을 해제한다.
		void Reset();
	};
}
